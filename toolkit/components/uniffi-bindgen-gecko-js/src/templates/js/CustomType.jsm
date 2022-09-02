class {{ ffi_converter }} extends FfiConverter {

    static lift(buf) {  
        return {{ builtin.ffi_converter() }}.lift(buf);    
    }
    
    static lower(buf) {
        return {{ builtin.ffi_converter() }}.lower(buf);
    }
    
    static write(dataStream, value) {
        {{ builtin.ffi_converter() }}.write(dataStream, value);
    } 
    
    static read(buf) {
        return {{ builtin.ffi_converter() }}.read(buf);
    }
    
    static computeSize(value) {
        return {{ builtin.ffi_converter() }}.computeSize(value);
    }
}
// TODO: We should also allow JS to customize the type eventually.