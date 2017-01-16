sub vpx_scale_forward_decls() {
print <<EOF
struct yv12_buffer_config;
EOF
}
forward_decls qw/vpx_scale_forward_decls/;

# Scaler functions
if (vpx_config("CONFIG_SPATIAL_RESAMPLING") eq "yes") {
    add_proto qw/void vp8_horizontal_line_5_4_scale/, "const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width";
    add_proto qw/void vp8_vertical_band_5_4_scale/, "unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width";
    add_proto qw/void vp8_horizontal_line_5_3_scale/, "const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width";
    add_proto qw/void vp8_vertical_band_5_3_scale/, "unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width";
    add_proto qw/void vp8_horizontal_line_2_1_scale/, "const unsigned char *source, unsigned int source_width, unsigned char *dest, unsigned int dest_width";
    add_proto qw/void vp8_vertical_band_2_1_scale/, "unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width";
    add_proto qw/void vp8_vertical_band_2_1_scale_i/, "unsigned char *source, unsigned int src_pitch, unsigned char *dest, unsigned int dest_pitch, unsigned int dest_width";
}

add_proto qw/void vp8_yv12_extend_frame_borders/, "struct yv12_buffer_config *ybf";

add_proto qw/void vp8_yv12_copy_frame/, "const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc";

add_proto qw/void vpx_yv12_copy_y/, "const struct yv12_buffer_config *src_ybc, struct yv12_buffer_config *dst_ybc";

if (vpx_config("CONFIG_VP9") eq "yes") {
    add_proto qw/void vpx_extend_frame_borders/, "struct yv12_buffer_config *ybf";
    specialize qw/vpx_extend_frame_borders dspr2/;

    add_proto qw/void vpx_extend_frame_inner_borders/, "struct yv12_buffer_config *ybf";
    specialize qw/vpx_extend_frame_inner_borders dspr2/;
}
1;
