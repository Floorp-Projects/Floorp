
// Read debug info from |obj_file| and park it in a Module, returned
// via |module|.  Caller owns the Module and is responsible for
// deallocating it.  Note that |debug_dirs| is ignored.
bool ReadSymbolData(const string& obj_file,
                    const std::vector<string> &debug_dirs,
                    SymbolData symbol_data,
                    google_breakpad::Module** module);
