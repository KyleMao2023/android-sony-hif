fn main() {
    for path in std::env::args().skip(1) {
        let bytes = std::fs::read(&path).expect("read image");
        println!("{path}: {:?}", sony_hif_codec::probe_bytes(&bytes));
    }
}
