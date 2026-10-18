//! The HEVC (ITU-T H.265) still-picture intra decoder.
//!
//! This module decodes one intra picture — Main, Main 10 or Main Still
//! Picture, 4:2:0, 4:2:2, 4:4:4 or monochrome, 8 or 10 bits — from the
//! parameter sets and coded slices of an IRAP access unit. Like every other
//! module in the crate it performs no I/O: [`decode_still`] takes bytes and
//! returns samples. Inter prediction, P and B slices and multi-picture
//! sequences are refused with [`Error::Unsupported`], naming the tool that was
//! asked for.
//!
//! [`Error::Unsupported`]: crate::Error::Unsupported
//!
//! # Contract
//!
//! `parameter_sets` are the VPS, SPS and PPS NAL units taken from the item's
//! `hvcC` property, in that order, each without a length prefix or start code.
//! `slices` are the remaining NAL units of the item's data, already split on
//! their length prefixes. Both borrow from the caller; the decoder does not
//! assume they outlive the call.
//!
//! # Errors
//!
//! The decoder has its own, narrower notion of failure, defined in the private
//! `error` submodule. It never reaches a caller: it is converted into
//! [`crate::Error`] at this seam, carrying the same message, so that a caller
//! of [`crate::decode()`] matches on exactly one error type.

mod bits;
mod cabac;
mod decode;
mod error;
mod filter;
mod frame;
mod intra;
mod nal;
mod picture;
mod ps;
mod scan;
mod slice;
mod transform;

#[cfg(any(test, feature = "bench"))]
#[doc(hidden)]
pub mod synth;

#[cfg(feature = "bench")]
#[doc(hidden)]
pub mod bench_api;

#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden_422;

pub use frame::{ChromaFormat, Frame, Info};

#[cfg(test)]
use nal::split_annexb;

use crate::error::Result;

/// Decode a single still picture.
///
/// See the module documentation for the shape of the inputs.
pub fn decode_still(parameter_sets: &[&[u8]], slices: &[&[u8]]) -> Result<Frame> {
    let sets = decode::ParameterSets::parse(parameter_sets)?;
    Ok(decode::decode(&sets, slices)?)
}

/// Allocation preflight for the exact SPS selected by the first coded slice.
/// Unlike `probe`, the returned dimensions include the conformance window.
/// This function allocates parameter/header data, never picture planes.
pub fn preflight_still(parameter_sets: &[&[u8]], slices: &[&[u8]]) -> Result<(u32, u32, Info)> {
    let sets = decode::ParameterSets::parse(parameter_sets)?;
    for unit in slices {
        let h = nal::NalHeader::parse(unit)?;
        if !h.is_vcl() || h.layer_id != 0 { continue; }
        if !h.is_irap() { return Err(error::Error::Unsupported("non-IRAP picture (inter prediction)").into()); }
        let rbsp=nal::Rbsp::from_nal(unit)?;
        let mut reader=bits::BitReader::new(&rbsp.data);
        reader.u1()?;
        reader.u1()?;
        let pps=sets.pps(reader.ue()?)?;
        let sps=sets.sps(pps.sps_id)?;
        let [l,r,t,b]=sps.crop;
        let crop_w=l.checked_add(r).ok_or(error::Error::TooLarge)?;
        let crop_h=t.checked_add(b).ok_or(error::Error::TooLarge)?;
        if crop_w>=sps.width || crop_h>=sps.height { return Err(error::Error::InvalidData("invalid conformance window").into()); }
        let coded_w=u32::try_from(sps.width).map_err(|_| error::Error::TooLarge)?;
        let coded_h=u32::try_from(sps.height).map_err(|_| error::Error::TooLarge)?;
        return Ok((coded_w,coded_h,Info {
            width:(sps.width-crop_w) as u32, height:(sps.height-crop_h) as u32,
            bit_depth:sps.bit_depth_y,
            chroma:ChromaFormat::from_idc(sps.chroma_array_type).ok_or(error::Error::InvalidData("chroma format"))?,
        }));
    }
    Err(error::Error::NoSlices.into())
}


/// Read the geometry out of a sequence parameter set without decoding.
pub fn probe(parameter_sets: &[&[u8]]) -> Result<Info> {
    let sets = decode::ParameterSets::parse(parameter_sets)?;
    let sps = sets.first_sps()?;
    let [l, r, t, b] = sps.crop;
    Ok(Info {
        width: sps.width.saturating_sub(l + r).max(1) as u32,
        height: sps.height.saturating_sub(t + b).max(1) as u32,
        bit_depth: sps.bit_depth_y,
        chroma: ChromaFormat::from_idc(sps.chroma_array_type).unwrap_or(ChromaFormat::Monochrome),
    })
}

/// Decode one still picture from an Annex-B byte stream.
///
/// Start codes are located, the NAL units split out and sorted into parameter
/// sets and slices, and [`decode_still`] is then applied. HEIF stores NAL
/// units with length prefixes rather than start codes, so this is used only by
/// the decoder's own tests, which speak the codec's native framing.
#[cfg(test)]
pub(crate) fn decode_annexb(stream: &[u8]) -> Result<Frame> {
    use alloc::vec::Vec;
    let units = split_annexb(stream);
    let mut sets: Vec<&[u8]> = Vec::new();
    let mut slices: Vec<&[u8]> = Vec::new();
    for u in units {
        let Ok(h) = nal::NalHeader::parse(u) else {
            continue;
        };
        if h.is_vcl() {
            slices.push(u);
        } else if matches!(h.nal_unit_type, 32..=34) {
            sets.push(u);
        }
    }
    decode_still(&sets, &slices)
}
