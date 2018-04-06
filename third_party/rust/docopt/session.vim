au BufWritePost *.rs silent!make ctags > /dev/null 2>&1
" let g:syntastic_rust_rustc_fname = "src/lib.rs" 
" let g:syntastic_rust_rustc_args = "--no-trans" 
