use super::crossroads::Crossroads;
use super::handlers::{ParInfo, Par};
use super::info::{IfaceInfo, MethodInfo, PropInfo};
use crate::arg;
use super::MethodErr;

pub struct DBusProperties;

impl DBusProperties {
    pub fn register(cr: &mut Crossroads<Par>) {
        cr.register_custom::<Self>(IfaceInfo::new("org.freedesktop.DBus.Properties",
            vec!(MethodInfo::new_par("Get", |_: &DBusProperties, info| {
                let (iname, propname) = info.msg().read2()?; 
                let (lookup, pinfo) = info.crossroads().reg_prop_lookup(info.path_data(), iname, propname)
                    .ok_or_else(|| { MethodErr::no_property(&"Could not find property") })?;
                let handler = &pinfo.handlers.0.as_ref()
                    .ok_or_else(|| { MethodErr::no_property(&"Property can not be read") })?;
                let iface = &**lookup.iface;
                let mut pinfo = ParInfo::new(info.msg(), lookup);
                let mut mret = info.msg().method_return();
                {
                    let mut ia = arg::IterAppend::new(&mut mret);
                    (handler)(iface, &mut ia, &mut pinfo)?;
                }
                Ok(Some(mret))
            })),
            vec!(), vec!()
        ));

    }
}

pub struct DBusIntrospectable;

use crate::crossroads as cr;

pub trait Introspectable {
    fn introspect(&self, info: &cr::ParInfo) -> Result<String, cr::MethodErr>;
}

pub fn introspectable_ifaceinfo<I>() -> cr::IfaceInfo<'static, cr::Par>
where I: Introspectable + Send + Sync + 'static {
    cr::IfaceInfo::new("org.freedesktop.DBus.Introspectable", vec!(
        MethodInfo::new_par("Introspect", |intf: &I, info| {
            let xml_data = intf.introspect(info)?;
            let rm = info.msg().method_return();
            let rm = rm.append1(xml_data);
            Ok(Some(rm))
       }),
    ), vec!(), vec!())
}


