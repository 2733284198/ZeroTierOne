use crate::*;
use crate::bindings::capi as ztcore;
use std::os::raw::{c_char, c_int};
use std::ffi::CStr;
use std::mem::MaybeUninit;

pub struct Fingerprint {
    pub address: Address,
    pub hash: [u8; 48]
}

impl Fingerprint {
    pub fn new_from_string(s: &str) -> Result<Fingerprint, ResultCode> {
        unsafe {
            let mut cfp: MaybeUninit<ztcore::ZT_Fingerprint> = MaybeUninit::uninit();
            if ztcore::ZT_Fingerprint_fromString(cfp.as_mut_ptr(), s.as_ptr() as *const c_char) != 0 {
                let fp = cfp.assume_init();
                return Ok(Fingerprint{
                    address: Address(fp.address),
                    hash: fp.hash
                });
            }
        }
        return Err(ResultCode::ErrorBadParameter);
    }
}

impl ToString for Fingerprint {
    fn to_string(&self) -> String {
        let mut buf: [u8; 256] = [0; 256];
        unsafe {
            if ztcore::ZT_Fingerprint_toString(&ztcore::ZT_Fingerprint {
                address: self.address.0,
                hash: self.hash
            }, buf.as_mut_ptr() as *mut c_char, buf.len() as c_int).is_null() {
                return String::from("(invalid)");
            }
            return String::from(CStr::from_bytes_with_nul(buf.as_ref()).unwrap().to_str().unwrap());
        }
    }
}

impl serde::Serialize for Fingerprint {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: serde::Serializer {
        serializer.serialize_str(self.to_string().as_str())
    }
}

struct FingerprintVisitor;

impl<'de> serde::de::Visitor<'de> for FingerprintVisitor {
    type Value = Fingerprint;

    fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        formatter.write_str("ZeroTier Fingerprint in string format")
    }

    fn visit_str<E>(self, s: &str) -> Result<Self::Value, E> where E: serde::de::Error {
        let id = Fingerprint::new_from_string(s);
        if id.is_err() {
            return Err(serde::de::Error::invalid_value(serde::de::Unexpected::Str(s), &self));
        }
        return Ok(id.ok().unwrap() as Self::Value);
    }
}

impl<'de> serde::Deserialize<'de> for Fingerprint {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: serde::Deserializer<'de> {
        deserializer.deserialize_str(FingerprintVisitor)
    }
}
