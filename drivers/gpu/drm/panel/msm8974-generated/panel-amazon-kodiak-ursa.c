// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct ursa {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
};

static inline struct ursa *to_ursa(struct drm_panel *panel)
{
	return container_of(panel, struct ursa, panel);
}

static void ursa_reset(struct ursa *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(200);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
}

static int ursa_on(struct ursa *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);
	mipi_dsi_dcs_set_tear_on_multi(&dsi_ctx, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	mipi_dsi_usleep_range(&dsi_ctx, 10000, 11000);

	return dsi_ctx.accum_err;
}

static int ursa_off(struct ursa *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x34, 0x00);
	mipi_dsi_usleep_range(&dsi_ctx, 10000, 11000);
	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 50);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int ursa_prepare(struct drm_panel *panel)
{
	struct ursa *ctx = to_ursa(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ursa_reset(ctx);

	ret = ursa_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	return 0;
}

static int ursa_unprepare(struct drm_panel *panel)
{
	struct ursa *ctx = to_ursa(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = ursa_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	return 0;
}

static const struct drm_display_mode ursa_mode = {
	.clock = (720 + 26 + 26 + 26) * (1280 + 2 + 2 + 2) * 60 / 1000,
	.hdisplay = 720,
	.hsync_start = 720 + 26,
	.hsync_end = 720 + 26 + 26,
	.htotal = 720 + 26 + 26 + 26,
	.vdisplay = 1280,
	.vsync_start = 1280 + 2,
	.vsync_end = 1280 + 2 + 2,
	.vtotal = 1280 + 2 + 2 + 2,
	.width_mm = 58,
	.height_mm = 103,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int ursa_get_modes(struct drm_panel *panel,
			  struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &ursa_mode);
}

static const struct drm_panel_funcs ursa_panel_funcs = {
	.prepare = ursa_prepare,
	.unprepare = ursa_unprepare,
	.get_modes = ursa_get_modes,
};

static int ursa_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ursa *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct ursa, panel,
				   &ursa_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void ursa_remove(struct mipi_dsi_device *dsi)
{
	struct ursa *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ursa_of_match[] = {
	{ .compatible = "amazon,kodiak-panel-ursa" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ursa_of_match);

static struct mipi_dsi_driver ursa_driver = {
	.probe = ursa_probe,
	.remove = ursa_remove,
	.driver = {
		.name = "panel-ursa",
		.of_match_table = ursa_of_match,
	},
};
module_mipi_dsi_driver(ursa_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for ursa 720p cmd mode dsi panel");
MODULE_LICENSE("GPL");
