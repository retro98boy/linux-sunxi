// SPDX-License-Identifier: GPL-2.0+
/*
 * This driver supports the analog controls for the internal codec
 * found in Allwinner's D1/T113s SoCs family.
 *
 * Based on sun50i-codec-analog.c
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>

/* Codec analog control register offsets and bit fields */
#define SUN20I_D1_ADDA_ADC1			(0x00)
#define SUN20I_D1_ADDA_ADC2			(0x04)
#define SUN20I_D1_ADDA_ADC3			(0x08)
#define SUN20I_D1_ADDA_ADC_EN				(31)
#define SUN20I_D1_ADDA_ADC_PGA_EN			(30)
#define SUN20I_D1_ADDA_ADC_MIC_SIN_EN			(28)
#define SUN20I_D1_ADDA_ADC_LINEINLEN			(23)
#define SUN20I_D1_ADDA_ADC_PGA_GAIN			(8)

#define SUN20I_D1_ADDA_DAC			(0x10)
#define SUN20I_D1_ADDA_DAC_DACL_EN			(15)
#define SUN20I_D1_ADDA_DAC_DACR_EN			(14)

#define SUN20I_D1_ADDA_MICBIAS			(0x18)
#define SUN20I_D1_ADDA_MICBIAS_MMICBIASEN		(7)

#define SUN20I_D1_ADDA_RAMP			(0x1C)
#define SUN20I_D1_ADDA_RAMP_RD_EN			(0)

#define SUN20I_D1_ADDA_HP2			(0x40)
#define SUN20I_D1_ADDA_HP2_HEADPHONE_GAIN		(28)

#define SUN20I_D1_ADDA_ADC_CUR_REG		(0x4C)

static const DECLARE_TLV_DB_RANGE(sun20i_d1_codec_adc_gain_scale,
	0, 0, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
	1, 3, TLV_DB_SCALE_ITEM(600, 0, 0),
	4, 4, TLV_DB_SCALE_ITEM(900, 0, 0),
	5, 31, TLV_DB_SCALE_ITEM(1000, 100, 0),
);

static const DECLARE_TLV_DB_SCALE(sun20i_d1_codec_hp_vol_scale, -4200, 600, 0);

/* volume controls */
static const struct snd_kcontrol_new sun20i_d1_codec_controls[] = {
	SOC_SINGLE_TLV("Headphone Playback Volume",
		       SUN20I_D1_ADDA_HP2,
		       SUN20I_D1_ADDA_HP2_HEADPHONE_GAIN, 0x7, 1,
		       sun20i_d1_codec_hp_vol_scale),
	SOC_SINGLE_TLV("ADC1 Gain Capture Volume",
		       SUN20I_D1_ADDA_ADC1,
		       SUN20I_D1_ADDA_ADC_PGA_GAIN, 0x1f, 0,
		       sun20i_d1_codec_adc_gain_scale),
	SOC_SINGLE_TLV("ADC2 Gain Capture Volume",
		       SUN20I_D1_ADDA_ADC2,
		       SUN20I_D1_ADDA_ADC_PGA_GAIN, 0x1f, 0,
		       sun20i_d1_codec_adc_gain_scale),
	SOC_SINGLE_TLV("ADC3 Gain Capture Volume",
		       SUN20I_D1_ADDA_ADC3,
		       SUN20I_D1_ADDA_ADC_PGA_GAIN, 0x1f, 0,
		       sun20i_d1_codec_adc_gain_scale),
};

/* ADC mixer controls */
static const struct snd_kcontrol_new sun20i_d1_codec_mixer_controls[] = {
	SOC_DAPM_DOUBLE_R("Line In Switch",
			  SUN20I_D1_ADDA_ADC1,
			  SUN20I_D1_ADDA_ADC2,
			  SUN20I_D1_ADDA_ADC_LINEINLEN, 1, 0),
};

static const char * const sun20i_d1_codec_mic3_src_enum_text[] = {
	"Differential", "Single",
};

static SOC_ENUM_SINGLE_DECL(sun20i_d1_codec_mic3_src_enum,
			    SUN20I_D1_ADDA_ADC3,
			    SUN20I_D1_ADDA_ADC_MIC_SIN_EN,
			    sun20i_d1_codec_mic3_src_enum_text);

static const struct snd_kcontrol_new sun20i_d1_codec_mic3_input_src[] = {
	SOC_DAPM_ENUM("MIC3 Source Capture Route",
		      sun20i_d1_codec_mic3_src_enum),
};

static const struct snd_soc_dapm_widget sun20i_d1_codec_widgets[] = {
	/* DAC */
	SND_SOC_DAPM_DAC("Left DAC", NULL, SUN20I_D1_ADDA_DAC,
			 SUN20I_D1_ADDA_DAC_DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", NULL, SUN20I_D1_ADDA_DAC,
			 SUN20I_D1_ADDA_DAC_DACR_EN, 0),
	/* ADC */
	SND_SOC_DAPM_ADC("ADC1", NULL, SUN20I_D1_ADDA_ADC1,
			 SUN20I_D1_ADDA_ADC_EN, 0),
	SND_SOC_DAPM_ADC("ADC2", NULL, SUN20I_D1_ADDA_ADC2,
			 SUN20I_D1_ADDA_ADC_EN, 0),
	SND_SOC_DAPM_ADC("ADC3", NULL, SUN20I_D1_ADDA_ADC3,
			 SUN20I_D1_ADDA_ADC_EN, 0),

	/* ADC Mixers */
	SND_SOC_DAPM_MIXER("ADC1 Mixer", SND_SOC_NOPM, 0, 0,
			   sun20i_d1_codec_mixer_controls,
			   ARRAY_SIZE(sun20i_d1_codec_mixer_controls)),
	SND_SOC_DAPM_MIXER("ADC2 Mixer", SND_SOC_NOPM, 0, 0,
			   sun20i_d1_codec_mixer_controls,
			   ARRAY_SIZE(sun20i_d1_codec_mixer_controls)),

	/* Headphone */
	SND_SOC_DAPM_OUTPUT("HP"),
	SND_SOC_DAPM_SUPPLY("RAMP Enable", SUN20I_D1_ADDA_RAMP,
			    SUN20I_D1_ADDA_RAMP_RD_EN, 0, NULL, 0),

	/* Line input */
	SND_SOC_DAPM_INPUT("LINEIN"),

	/* Microphone input */
	SND_SOC_DAPM_INPUT("MIC3"),

	/* Microphone input path */
	SND_SOC_DAPM_MUX("MIC3 Source Capture Route", SND_SOC_NOPM, 0, 0,
			 sun20i_d1_codec_mic3_input_src),

	SND_SOC_DAPM_PGA("Mic3 Amplifier", SUN20I_D1_ADDA_ADC3,
			 SUN20I_D1_ADDA_ADC_PGA_EN, 0, NULL, 0),

	/* Microphone Bias */
	SND_SOC_DAPM_SUPPLY("MBIAS", SUN20I_D1_ADDA_MICBIAS,
			    SUN20I_D1_ADDA_MICBIAS_MMICBIASEN, 0, NULL, 0),
};

static const struct snd_soc_dapm_route sun20i_d1_codec_routes[] = {
	/* Headphone Routes */
	{ "HP", NULL, "Left DAC" },
	{ "HP", NULL, "Right DAC" },
	{ "HP", NULL, "RAMP Enable" },

	/* Line input Routes */
	{ "ADC1", NULL, "ADC1 Mixer" },
	{ "ADC2", NULL, "ADC2 Mixer" },
	{ "ADC1 Mixer", "Line In Switch", "LINEIN" },
	{ "ADC2 Mixer", "Line In Switch", "LINEIN" },

	/* Microphone Routes */
	{ "MIC3 Source Capture Route", "Differential", "MIC3" },
	{ "MIC3 Source Capture Route", "Single", "MIC3" },
	{ "Mic3 Amplifier", NULL, "MIC3 Source Capture Route" },
	{ "ADC3", NULL, "Mic3 Amplifier" },
};

static const struct snd_soc_component_driver sun20i_d1_codec_analog_cmpnt_drv = {
	.controls		= sun20i_d1_codec_controls,
	.num_controls		= ARRAY_SIZE(sun20i_d1_codec_controls),
	.dapm_widgets		= sun20i_d1_codec_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(sun20i_d1_codec_widgets),
	.dapm_routes		= sun20i_d1_codec_routes,
	.num_dapm_routes	= ARRAY_SIZE(sun20i_d1_codec_routes),
};

static const struct of_device_id sun20i_d1_codec_analog_of_match[] = {
	{
		.compatible = "allwinner,sun20i-d1-codec-analog",
	},
	{}
};
MODULE_DEVICE_TABLE(of, sun20i_d1_codec_analog_of_match);

static const struct regmap_config sun20i_d1_codec_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= SUN20I_D1_ADDA_ADC_CUR_REG,
};

static int sun20i_d1_codec_analog_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	void __iomem *base;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "Failed to map the registers\n");
		return PTR_ERR(base);
	}

	regmap = devm_regmap_init_mmio(&pdev->dev, base,
				       &sun20i_d1_codec_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "Failed to create regmap\n");
		return PTR_ERR(regmap);
	}

	return devm_snd_soc_register_component(&pdev->dev,
					       &sun20i_d1_codec_analog_cmpnt_drv,
					       NULL, 0);
}

static struct platform_driver sun20i_d1_codec_analog_driver = {
	.driver = {
		.name = "sun20i-d1-codec-analog",
		.of_match_table = sun20i_d1_codec_analog_of_match,
	},
	.probe = sun20i_d1_codec_analog_probe,
};
module_platform_driver(sun20i_d1_codec_analog_driver);

MODULE_DESCRIPTION("Allwinner internal codec analog controls driver for D1");
MODULE_AUTHOR("Maksim Kiselev <bigunclemax@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sun20i-d1-codec-analog");
