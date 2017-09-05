mat!(uni_literal, r"☃", "☃", Some((0, 3)));
mat!(uni_literal_plus, r"☃+", "☃", Some((0, 3)));
mat!(uni_literal_casei_plus, r"(?i)☃+", "☃", Some((0, 3)));
mat!(uni_class_plus, r"[☃Ⅰ]+", "☃", Some((0, 3)));
mat!(uni_one, r"\pN", "Ⅰ", Some((0, 3)));
mat!(uni_mixed, r"\pN+", "Ⅰ1Ⅱ2", Some((0, 8)));
mat!(uni_not, r"\PN+", "abⅠ", Some((0, 2)));
mat!(uni_not_class, r"[\PN]+", "abⅠ", Some((0, 2)));
mat!(uni_not_class_neg, r"[^\PN]+", "abⅠ", Some((2, 5)));
mat!(uni_case, r"(?i)Δ", "δ", Some((0, 2)));
mat!(uni_case_upper, r"\p{Lu}+", "ΛΘΓΔα", Some((0, 8)));
mat!(uni_case_upper_nocase_flag, r"(?i)\p{Lu}+", "ΛΘΓΔα", Some((0, 10)));
mat!(uni_case_upper_nocase, r"\p{L}+", "ΛΘΓΔα", Some((0, 10)));
mat!(uni_case_lower, r"\p{Ll}+", "ΛΘΓΔα", Some((8, 10)));

// Test the Unicode friendliness of Perl character classes.
mat!(uni_perl_w, r"\w+", "dδd", Some((0, 4)));
mat!(uni_perl_w_not, r"\w+", "⥡", None);
mat!(uni_perl_w_neg, r"\W+", "⥡", Some((0, 3)));
mat!(uni_perl_d, r"\d+", "1२३9", Some((0, 8)));
mat!(uni_perl_d_not, r"\d+", "Ⅱ", None);
mat!(uni_perl_d_neg, r"\D+", "Ⅱ", Some((0, 3)));
mat!(uni_perl_s, r"\s+", " ", Some((0, 3)));
mat!(uni_perl_s_not, r"\s+", "☃", None);
mat!(uni_perl_s_neg, r"\S+", "☃", Some((0, 3)));

// And do the same for word boundaries.
mat!(uni_boundary_none, r"\d\b", "6δ", None);
mat!(uni_boundary_ogham, r"\d\b", "6 ", Some((0, 1)));
mat!(uni_not_boundary_none, r"\d\B", "6δ", Some((0, 1)));
mat!(uni_not_boundary_ogham, r"\d\B", "6 ", None);
