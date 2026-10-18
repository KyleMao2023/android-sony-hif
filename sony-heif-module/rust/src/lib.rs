#[cfg(test)] use std::path::PathBuf;
use heic_rs::{context::Context, props::colr::Nclx, ChromaFormat};
use std::{ffi::{c_char,CString},cell::RefCell,panic::{catch_unwind,AssertUnwindSafe},sync::{OnceLock,atomic::{AtomicBool,Ordering}}};

#[derive(Debug, Default, Clone, Copy, PartialEq)]
#[repr(C)]
pub struct HifInfo {
    pub width: u32,
    pub height: u32,
    pub bit_depth: u32,
    pub chroma: u32,
    pub hlg: u32,
    pub grid_rows: u32,
    pub grid_columns: u32,
    pub tile_width: u32,
    pub tile_height: u32,
}

pub const MAX_INPUT: usize = 128 * 1024 * 1024;
pub const MAX_PIXELS: u64 = 80_000_000;
const MAX_TILE_PIXELS:u64=16_000_000;
const MAX_TILE_COUNT:usize=64;

// This reservation survives decoding and is released only with the RGB16 buffer.
// Admission fails immediately, so one live decoder cannot deadlock another.
struct DecodeReservation(&'static AtomicBool);
impl DecodeReservation {
    fn acquire(slot:&'static AtomicBool)->Result<Self,String>{
        slot.compare_exchange(false,true,Ordering::AcqRel,Ordering::Acquire)
            .map(|_|Self(slot)).map_err(|_|"another decoded image is still live".into())
    }
}
impl Drop for DecodeReservation {fn drop(&mut self){self.0.store(false,Ordering::Release);}}
static DECODE_SLOT:AtomicBool=AtomicBool::new(false);

fn check_coded_size(width:u32,height:u32,total:&mut u64)->Result<(),String>{
    let pixels=u64::from(width)*u64::from(height);
    if width==0 || height==0 || width>8192 || height>8192 || pixels>MAX_TILE_PIXELS {
        return Err("coded tile size limit".into());
    }
    *total=total.checked_add(pixels).ok_or("tile pixel overflow")?;
    if *total>MAX_PIXELS {return Err("total coded tile pixel limit".into());}
    Ok(())
}

fn preflight_tiles(ctx:&Context<'_>,p:&heic_rs::ImageInfo)->Result<(),String>{
    if u64::from(p.coded_width)*u64::from(p.coded_height)>MAX_PIXELS {return Err("coded output pixel limit".into());}
    let derivation=ctx.grid(ctx.meta.primary).map_err(|e|e.to_string())?;
    let tiles=if let Some((g,ids))=&derivation {
        if (g.output_width,g.output_height)!=(p.coded_width,p.coded_height) {return Err("grid and primary size differ".into());}
        ids.clone()
    } else {vec![ctx.meta.primary]};
    if tiles.is_empty() || tiles.len()>MAX_TILE_COUNT {return Err("tile count limit".into());}
    let mut total=0;
    let mut first=None;
    for id in tiles {
        let props=ctx.props(id).map_err(|e|e.to_string())?;
        let hvcc=props.hvcc.as_ref().ok_or("tile hvcC missing")?;
        let data=ctx.item_data(id).map_err(|e|e.to_string())?;
        let slices=hvcc.split_nals(&data).map_err(|e|e.to_string())?;
        let (cw,ch,actual)=heic_rs::hevc::preflight_still(&hvcc.parameter_sets(),&slices).map_err(|e|e.to_string())?;
        check_coded_size(cw,ch,&mut total)?;
        if !matches!(actual.bit_depth,8|10) || !matches!(actual.chroma,ChromaFormat::Yuv420|ChromaFormat::Yuv422) {
            return Err("unsupported coded pixel format".into());
        }
        let declared=ctx.coded_size(id,&props).map_err(|e|e.to_string())?;
        if declared!=(actual.width,actual.height) {return Err("SPS and tile size differ".into());}
        if (p.bit_depth,p.chroma)!=(actual.bit_depth,actual.chroma) {return Err("SPS and primary pixel format differ".into());}
        let key=(actual.width,actual.height,actual.bit_depth,actual.chroma);
        if first.is_some_and(|v|v!=key) {return Err("inconsistent tile geometry or format".into());}
        first=Some(key);
    }
    if let Some((g,_))=derivation {
        let (tw,th,_,_)=first.ok_or("no tiles")?;
        let (w,h)=(u64::from(g.output_width),u64::from(g.output_height));
        let (cols,rows)=(u64::from(g.columns),u64::from(g.rows));
        // Only the final column/row may overhang the output. Repeated tiles
        // still count separately against the coded budget above.
        if w<=u64::from(tw)*(cols-1) || w>u64::from(tw)*cols || h<=u64::from(th)*(rows-1) || h>u64::from(th)*rows {
            return Err("grid footprint is inconsistent".into());
        }
    }
    Ok(())
}

fn encoded_color(bytes: &[u8]) -> Result<Nclx, String> {
    let ctx = Context::open(bytes).map_err(|e| e.to_string())?;
    let primary = ctx.props(ctx.meta.primary).map_err(|e| e.to_string())?;
    if let Some(nclx) = primary.nclx { return Ok(nclx); }
    if let Some((_, tiles)) = ctx.grid(ctx.meta.primary).map_err(|e| e.to_string())? {
        if let Some(&first) = tiles.first() {
            if let Some(nclx) = ctx.props(first).map_err(|e| e.to_string())?.nclx { return Ok(nclx); }
        }
    }
    Ok(Nclx::default())
}

pub fn probe_bytes(bytes: &[u8]) -> Result<HifInfo, String> {
    if bytes.is_empty() || bytes.len() > MAX_INPUT { return Err("input size limit".into()); }
    let p = heic_rs::probe(bytes).map_err(|e| e.to_string())?;
    if p.width == 0 || p.height == 0 || u64::from(p.width) * u64::from(p.height) > MAX_PIXELS {
        return Err("pixel limit".into());
    }
    preflight_tiles(&Context::open(bytes).map_err(|e|e.to_string())?,&p)?;
    let color = encoded_color(bytes)?;
    if !matches!(color.transfer, 1 | 2 | 13 | 18) { return Err("unsupported transfer curve".into()); }
    let chroma = match p.chroma { ChromaFormat::Monochrome => 0, ChromaFormat::Yuv420 => 1,
        ChromaFormat::Yuv422 => 2, ChromaFormat::Yuv444 => 3 };
    let g = p.grid;
    Ok(HifInfo { width:p.width, height:p.height, bit_depth:p.bit_depth.into(), chroma,
        hlg:u32::from(color.transfer == 18), grid_rows:g.map_or(1, |g|g.rows),
        grid_columns:g.map_or(1, |g|g.columns), tile_width:g.map_or(p.width, |g|g.tile_width),
        tile_height:g.map_or(p.height, |g|g.tile_height) })
}

pub struct Decoded { pub info:HifInfo, pub pixels:Vec<u8>, _reservation:Option<DecodeReservation> }
pub fn decode_bytes(bytes: &[u8]) -> Result<Decoded, String> {
    let reservation=DecodeReservation::acquire(&DECODE_SLOT)?;
    let info=probe_bytes(bytes)?;
    let mut options=heic_rs::DecodeOptions::default().with_layout(heic_rs::PixelLayout::Rgb16)
        .with_max_pixels(Some(MAX_PIXELS)).with_alpha(false);
    options.threads=Some(2);
    let image=heic_rs::decode(bytes,&options).map_err(|e|e.to_string())?;
    if (image.width,image.height)!=(info.width,info.height) { return Err("decoded dimensions differ from metadata".into()); }
    Ok(Decoded{info,pixels:image.data,_reservation:Some(reservation)})
}

struct HlgTables { inverse:Vec<f32>, output:Vec<u16> }
static HLG_TABLES:OnceLock<HlgTables> = OnceLock::new();
fn hlg_tables() -> &'static HlgTables {
    HLG_TABLES.get_or_init(|| {
        let mut inverse=Vec::with_capacity(65536);
        let mut output=Vec::with_capacity(65536);
        for i in 0..=65535 {
            let v=i as f32/65535.0;
            inverse.push(if v<=0.5 {v*v/3.0} else {(((v-0.55991073)/0.17883277).exp()+0.28466892)/12.0});
            // Deliberate SDR viewing curve: exposure 2.4, ACES fitted curve, sRGB OETF.
            // This is not a calibrated BT.2100 display rendering or Sony-equivalent transform.
            let x=v*2.0*2.4;
            let linear=((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14)).clamp(0.0,1.0);
            let s=if linear<=0.0031308 {12.92*linear} else {1.055*linear.powf(1.0/2.4)-0.055};
            output.push((s.clamp(0.0,1.0)*65535.0+0.5) as u16);
        }
        HlgTables{inverse,output}
    })
}
pub fn hlg_to_sdr(rgb:[u16;3]) -> [u16;3] {
    let t=hlg_tables();
    let [r,g,b]=rgb.map(|v|t.inverse[v as usize]);
    let linear=[1.6605*r-0.5876*g-0.0728*b,-0.1246*r+1.1329*g-0.0083*b,-0.0182*r-0.1006*g+1.1187*b];
    linear.map(|v|t.output[(v.clamp(0.0,2.0)*32767.5+0.5) as usize])
}
pub fn write_row(image:&Decoded, row:u32, format:u32, dst:&mut[u8]) -> Result<(),String> {
    let bpp=match format {0=>2,1..=3=>4,_=>return Err("unsupported output format".into())};
    if row>=image.info.height || dst.len()<image.info.width as usize*bpp {return Err("row buffer bounds".into());}
    let start=row as usize*image.info.width as usize*6;
    let src=&image.pixels[start..start+image.info.width as usize*6];
    for (p,out) in src.chunks_exact(6).zip(dst.chunks_exact_mut(bpp)) {
        let mut rgb=[u16::from_ne_bytes([p[0],p[1]]),u16::from_ne_bytes([p[2],p[3]]),u16::from_ne_bytes([p[4],p[5]])];
        if image.info.hlg!=0 {rgb=hlg_to_sdr(rgb);}
        let [r,g,b]=rgb.map(u32::from);
        let q=|v:u32,m:u32| (v*m+32767)/65535;
        match format {
            0=>out.copy_from_slice(&((q(r,31)<<11|q(g,63)<<5|q(b,31)) as u16).to_ne_bytes()),
            1=>out.copy_from_slice(&[q(r,255) as u8,q(g,255) as u8,q(b,255) as u8,255]),
            2=>out.copy_from_slice(&[q(b,255) as u8,q(g,255) as u8,q(r,255) as u8,255]),
            3=>out.copy_from_slice(&(q(r,1023)|q(g,1023)<<10|q(b,1023)<<20|3<<30).to_ne_bytes()),
            _=>unreachable!(),
        }
    }
    Ok(())
}

thread_local! {static LAST_ERROR:RefCell<CString>=RefCell::new(CString::new("").unwrap());}
fn report_error(e:impl std::fmt::Display) {LAST_ERROR.with(|s|*s.borrow_mut()=CString::new(e.to_string().replace('\0'," ")).unwrap());}
#[unsafe(no_mangle)]
pub extern "C" fn sony_hif_last_error() -> *const c_char { LAST_ERROR.with(|s|s.borrow().as_ptr()) }

#[unsafe(no_mangle)]
pub unsafe extern "C" fn sony_hif_probe(data:*const u8,len:usize,out:*mut HifInfo) -> i32 {
    if data.is_null() || out.is_null() || len==0 || len>MAX_INPUT {report_error("invalid probe arguments");return -1;}
    match catch_unwind(AssertUnwindSafe(||probe_bytes(unsafe{std::slice::from_raw_parts(data,len)}))) {
        Ok(Ok(info))=>{unsafe{out.write(info)};0},Ok(Err(e))=>{report_error(e);-1},Err(_)=>{report_error("decoder panic contained");-1}
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sony_hif_decode(data:*const u8,len:usize) -> *mut Decoded {
    if data.is_null() || len==0 || len>MAX_INPUT {report_error("invalid decode arguments");return std::ptr::null_mut();}
    match catch_unwind(AssertUnwindSafe(||decode_bytes(unsafe{std::slice::from_raw_parts(data,len)}))) {
        Ok(Ok(image))=>Box::into_raw(Box::new(image)),Ok(Err(e))=>{report_error(e);std::ptr::null_mut()},
        Err(_)=>{report_error("decoder panic contained");std::ptr::null_mut()}
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sony_hif_row(image:*const Decoded,row:u32,format:u32,dst:*mut u8,len:usize) -> i32 {
    if image.is_null() || dst.is_null() {return -1;}
    match catch_unwind(AssertUnwindSafe(||write_row(unsafe{&*image},row,format,unsafe{std::slice::from_raw_parts_mut(dst,len)}))) {
        Ok(Ok(()))=>0,Ok(Err(e))=>{report_error(e);-1},Err(_)=>{report_error("row panic contained");-1}
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn sony_hif_free(image:*mut Decoded) {if !image.is_null(){drop(unsafe{Box::from_raw(image)});}}

#[cfg(test)]
pub fn sample_path(name: &str) -> PathBuf {
    std::env::var_os("SONY_HIF_SAMPLES").map(PathBuf::from).unwrap_or_else(||
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../heic-android-audit/samples")
    ).join(name)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn probes_real_sony_primary_and_grid() {
        for (name, chroma, hlg) in [("hif420.HIF", 1, 0), ("hif422.HIF", 2, 0), ("hif422hlg.HIF", 2, 1)] {
            let bytes = std::fs::read(sample_path(name)).expect("Sony sample required");
            let got = probe_bytes(&bytes).expect("must probe the primary Sony image");
            assert_eq!(got, HifInfo { width: 9504, height: 6336, bit_depth: 10, chroma, hlg,
                grid_rows: 3, grid_columns: 3, tile_width: 3200, tile_height: 2112 });
        }
    }

    #[test]
    fn malformed_input_is_an_error() {
        assert!(probe_bytes(b"not a HEIF image").is_err());
    }

    #[test]
    fn rejects_grid_payload_that_disagrees_with_primary_size() {
        let mut bytes=std::fs::read(sample_path("hif422.HIF")).unwrap();
        let ctx=Context::open(&bytes).unwrap();
        let data=ctx.item_data(ctx.meta.primary).unwrap();
        let offset=data.as_ptr() as usize-bytes.as_ptr() as usize;
        assert!(offset<bytes.len());
        // Sony's 16-bit grid output width becomes 1 while ispe still says 9504.
        assert_eq!(data[1]&1,0);
        drop(data);drop(ctx);
        bytes[offset+4..offset+6].copy_from_slice(&1u16.to_be_bytes());
        assert!(probe_bytes(&bytes).is_err(),"inconsistent grid must fail before pixel allocation");
    }

    #[test]
    fn preflights_the_last_tile_not_only_the_first() {
        let mut bytes=std::fs::read(sample_path("hif422.HIF")).unwrap();
        let offset={
            let ctx=Context::open(&bytes).unwrap();
            let (_,ids)=ctx.grid(ctx.meta.primary).unwrap().unwrap();
            let id=*ids.last().unwrap();
            let props=ctx.props(id).unwrap();
            let data=ctx.item_data(id).unwrap();
            let nals=props.hvcc.unwrap().split_nals(&data).unwrap();
            nals[0].as_ptr() as usize-bytes.as_ptr() as usize
        };
        // Corrupt only the last coded tile, leaving container metadata intact.
        bytes[offset]|=0x80;
        assert!(heic_rs::probe(&bytes).is_ok());
        assert!(probe_bytes(&bytes).is_err());
    }

    #[test]
    fn coded_budget_counts_references_and_uncropped_sps_dimensions() {
        let mut total=0;
        for _ in 0..11 {check_coded_size(3200,2112,&mut total).unwrap();}
        assert!(check_coded_size(3200,2112,&mut total).is_err());
        assert!(check_coded_size(8192,8192,&mut 0).is_err());
        assert!(check_coded_size(16384,64,&mut 0).is_err());
    }

    #[test]
    fn actual_sps_dimensions_are_rejected_before_picture_allocation() {
        use heic_rs::hevc::{synth,preflight_still};
        let sets=[synth::to_nal(32,&synth::build_vps()),
            synth::to_nal(33,&synth::build_sps(16384,8192,2,10)),
            synth::to_nal(34,&synth::build_pps())];
        let params:Vec<&[u8]>=sets.iter().map(|v|v.as_slice()).collect();
        // The deliberately incomplete slice provides only PPS selection bits.
        // Preflight reads its SPS without allocating/decoding a huge picture.
        let slice=synth::to_nal(19,&[0xa0]);
        let (cw,ch,info)=preflight_still(&params,&[&slice]).unwrap();
        assert_eq!((cw,ch,info.width,info.height),(16384,8192,16384,8192));
        assert!(check_coded_size(cw,ch,&mut 0).is_err());
    }

    #[test]
    fn live_reservation_fails_fast_and_recovers_after_drop() {
        static SLOT:AtomicBool=AtomicBool::new(false);
        let first=DecodeReservation::acquire(&SLOT).unwrap();
        assert!(DecodeReservation::acquire(&SLOT).is_err());
        drop(first);
        assert!(DecodeReservation::acquire(&SLOT).is_ok());
        assert!(DecodeReservation::acquire(&SLOT).is_ok());
    }

    #[test]
    fn packs_android_pixel_formats_and_checks_row_bounds() {
        let pixels = [65535u16, 0, 32768].into_iter().flat_map(u16::to_ne_bytes).collect();
        let image=Decoded{info:HifInfo{width:1,height:1,..Default::default()},pixels,_reservation:None};
        let mut dst=[0u8;4];
        write_row(&image,0,1,&mut dst).unwrap(); assert_eq!(dst,[255,0,128,255]);
        write_row(&image,0,2,&mut dst).unwrap(); assert_eq!(dst,[128,0,255,255]);
        write_row(&image,0,3,&mut dst).unwrap();
        assert_eq!(u32::from_ne_bytes(dst), 1023 | (512 << 20) | (3 << 30));
        write_row(&image,0,0,&mut dst[..2]).unwrap(); assert_eq!(u16::from_ne_bytes([dst[0],dst[1]]), 0xf810);
        assert!(write_row(&image,1,1,&mut dst).is_err());
        assert!(write_row(&image,0,1,&mut dst[..3]).is_err());
        assert!(write_row(&image,0,99,&mut dst).is_err());
    }

    #[test]
    fn hlg_viewing_transform_is_finite_neutral_and_monotonic() {
        assert_eq!(hlg_to_sdr([0;3]),[0;3]);
        let mid=hlg_to_sdr([32768;3]);
        assert!((38000..39000).contains(&mid[0]),"HLG 0.5 through documented viewing curve: {mid:?}");
        assert!(mid.iter().max().unwrap()-mid.iter().min().unwrap() < 20);
        let mut previous=0;
        for v in (0..=65535).step_by(1024) { let out=hlg_to_sdr([v;3]); assert!(out[0]>=previous); previous=out[0]; }
    }

    #[test]
    #[ignore = "full-resolution acceptance; run explicitly in release mode"]
    fn decodes_all_three_full_resolution_primaries() {
        for name in ["hif420.HIF", "hif422.HIF", "hif422hlg.HIF"] {
            let bytes=std::fs::read(sample_path(name)).unwrap();
            let image=decode_bytes(&bytes).expect("primary full decode");
            assert_eq!((image.info.width,image.info.height),(9504,6336));
            assert_eq!(image.pixels.len(),9504*6336*6);
            assert!(image.pixels.iter().any(|&v|v != 0));
            assert!(decode_bytes(&bytes).is_err(),"retained image must reserve the sole decode slot");
        }
    }
}
