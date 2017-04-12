SCALE_SRCS-yes += aom_scale.mk
SCALE_SRCS-yes += yv12config.h
SCALE_SRCS-$(CONFIG_SPATIAL_RESAMPLING) += aom_scale.h
SCALE_SRCS-$(CONFIG_SPATIAL_RESAMPLING) += generic/aom_scale.c
SCALE_SRCS-yes += generic/yv12config.c
SCALE_SRCS-yes += generic/yv12extend.c
SCALE_SRCS-$(CONFIG_SPATIAL_RESAMPLING) += generic/gen_scalers.c
SCALE_SRCS-yes += aom_scale_rtcd.c
SCALE_SRCS-yes += aom_scale_rtcd.pl

#mips(dspr2)
SCALE_SRCS-$(HAVE_DSPR2)  += mips/dspr2/yv12extend_dspr2.c

SCALE_SRCS-no += $(SCALE_SRCS_REMOVE-yes)

$(eval $(call rtcd_h_template,aom_scale_rtcd,aom_scale/aom_scale_rtcd.pl))
