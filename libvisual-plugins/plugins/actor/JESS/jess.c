/*
 * Ported to the libvisual system by Dennis Smit  <ds@nerds-incorporated.org>
 *
 * Any questions about the port or code that comes from the libvisual source tree
 * should be written to Dennis Smit. Not Remi Arquier.
 *
 * JESS v2.9.1
 *
 *        Copyright (c) 2000 by Remi Arquier
 * Freely redistributable under the Gnu GPL version 2 or later
 *
 *                    <arquier@crans.org>
 *                   http://arquier.free.fr
 *                         17 dec 2001
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>  

#include "def.h"
#include "struct.h"
#include "distorsion.h"
#include "analyser.h"
#include "analyser_struct.h"
#include "renderer.h"
#include "pal.h"
#include "draw_low_level.h"
#include "jess.h"

int act_jess_init (VisActorPlugin *plugin);
int act_jess_cleanup (VisActorPlugin *plugin);
int act_jess_requisition (VisActorPlugin *plugin, int *width, int *height);
int act_jess_dimension (VisActorPlugin *plugin, VisVideo *video, int width, int height);
int act_jess_events (VisActorPlugin *plugin, VisEventQueue *events);
VisPalette *act_jess_palette (VisActorPlugin *plugin);
int act_jess_render (VisActorPlugin *plugin, VisVideo *video, VisAudio *audio);

void jess_init (JessPrivate *priv);

LVPlugin *get_plugin_info (VisPluginRef *ref)
{
	LVPlugin *plugin;
	VisActorPlugin *jess;
	JessPrivate *priv;

	plugin = visual_plugin_new ();
	if (plugin == NULL) {
		visual_log (VISUAL_LOG_CRITICAL,
			"Could not create a new plugin");
		return NULL;
	}

	jess = visual_plugin_actor_new ();
	if (jess == NULL) {
		visual_log (VISUAL_LOG_CRITICAL,
			"Could not create a new actor plugin");
		return NULL;
	}

	jess->name = "jess";
	jess->info = visual_plugin_info_new (
			"jess plugin",
			"Original by: Remi Arquier <arquier@crans.org>, Port by: Dennis Smit <ds@nerds-incorporated.org>",
			"0.1",
			"The jess visual plugin",
			"This is the libvisual plugin for the jess visual.");

	jess->init =		act_jess_init;
	jess->cleanup =		act_jess_cleanup;
	jess->requisition = 	act_jess_requisition;
	jess->events =		act_jess_events;
	jess->palette =		act_jess_palette;
	jess->render =		act_jess_render;

	jess->depth =
		VISUAL_VIDEO_DEPTH_8BIT |
		VISUAL_VIDEO_DEPTH_32BIT;

	priv = malloc (sizeof (JessPrivate));
	memset (priv, 0, sizeof (JessPrivate));

	jess->private = priv;
	
	plugin->type = VISUAL_PLUGIN_TYPE_ACTOR;
	plugin->plugin.actorplugin = jess;

	return plugin;
}

int act_jess_init (VisActorPlugin *plugin)
{
	JessPrivate *priv;

	visual_log_return_val_if_fail (plugin != NULL, -1);

	priv = plugin->private;
	if (priv == NULL) {
		visual_log (VISUAL_LOG_CRITICAL,
				"The given plugin doesn't have private info");
		return -1;
	}

	priv->conteur.burn_mode = 4;
	priv->conteur.draw_mode = 4;
	priv->conteur.blur_mode = 3;
	priv->video = 8;
	priv->conteur.term_display = OUI;
	priv->resx = 320;
	priv->resy = 200;

	priv->conteur.courbe = 0;
	priv->conteur.angle = 0;
	priv->conteur.angle2 = 0;
	priv->conteur.k1 = 0;
	priv->conteur.k2 = 0;
	priv->conteur.k3 = 10000;

	priv->conteur.fps = 40;

	priv->conteur.mix_reprise = 1000;
	priv->conteur.last_flash = 1000;
	priv->conteur.burn_mode = 2;
	priv->conteur.draw_mode = 5;
	priv->conteur.v_angle2 = 1;
	priv->conteur.general = 0;
	priv->conteur.blur_mode = 3;
	priv->conteur.freeze = 0;
	priv->conteur.freeze_mode = 0;

	priv->lys.E_moyen = 0;
	priv->lys.dEdt_moyen = 0;

	start_ticks (priv);
	return 0;
}

int act_jess_cleanup (VisActorPlugin *plugin)
{
	JessPrivate *priv;
	int i;

	visual_log_return_val_if_fail (plugin != NULL, -1);

	priv = plugin->private;
	if (priv == NULL) {
		visual_log (VISUAL_LOG_CRITICAL,
				"The given plugin doesn't have private info");
		return -1;
	}

	if (priv->big_ball != NULL)
		free (priv->big_ball);

	for (i = 0; i < BIG_BALL_SIZE; i++)
	{
		if (priv->big_ball_scale[i] != NULL)
			free (priv->big_ball_scale[i]);
	}

	free (priv->table1);
	free (priv->table2);
	free (priv->table3);
	free (priv->table4);
	free (priv->buffer);

	free (priv);
	
	return 0;
}

int act_jess_requisition (VisActorPlugin *plugin, int *width, int *height)
{
	int reqw, reqh;

	visual_log_return_val_if_fail (width != NULL, -1);
	visual_log_return_val_if_fail (height != NULL, -1);

	reqw = *width;
	reqh = *height;

	while (reqw % 2 || (reqw / 2) % 2)
		reqw--;
	
	while (reqh % 2 || (reqh / 2) % 2)
		reqh--;

	if (reqw < 32)
		reqw = 32;

	if (reqh < 32)
		reqh = 32;

	*width = reqw;
	*height = reqh;

	return 0;
}

int act_jess_dimension (VisActorPlugin *plugin, VisVideo *video, int width, int height)
{
	JessPrivate *priv;

	visual_log_return_val_if_fail (plugin != NULL, -1);

	priv = plugin->private;
	if (priv == NULL) {
		visual_log (VISUAL_LOG_CRITICAL,
				"The given plugin doesn't have private info");
		return -1;
	}

	priv->resx = width;
	priv->resy = height;

	visual_video_set_dimension (video, width, height);
	
	free (priv->table1);
	free (priv->table2);
	free (priv->table3);
	free (priv->table4);
	free (priv->buffer);

	priv->pitch = video->pitch;
	priv->video = visual_video_depth_value_from_enum (video->depth);
	priv->bpp = video->bpp;

	ball_init (priv);
	jess_init (priv);

	return 0;
}

int act_jess_events (VisActorPlugin *plugin, VisEventQueue *events)
{
	VisEvent ev;

	while (visual_event_queue_poll (events, &ev)) {
		switch (ev.type) {
			case VISUAL_EVENT_RESIZE:
				act_jess_dimension (plugin, ev.resize.video,
						ev.resize.width, ev.resize.height);
				break;
			default: /* to avoid warnings */
				break;
		}
	}

	return 0;
}

VisPalette *act_jess_palette (VisActorPlugin *plugin)
{
	JessPrivate *priv;

	visual_log_return_val_if_fail (plugin != NULL, NULL);

	priv = plugin->private;
	if (priv == NULL) {
		visual_log (VISUAL_LOG_CRITICAL,
				"The given plugin doesn't have private info");
		return NULL;
	}

	return &priv->jess_pal;
}

int act_jess_render (VisActorPlugin *plugin, VisVideo *video, VisAudio *audio)
{
	JessPrivate *priv;
	short freqdata[2][256];
	int i;

	visual_log_return_val_if_fail (plugin != NULL, -1);
	visual_log_return_val_if_fail (audio != NULL, -1);
	visual_log_return_val_if_fail (video != NULL, -1);

	priv = plugin->private;
	if (priv == NULL) {
		visual_log (VISUAL_LOG_CRITICAL,
				"The given plugin doesn't have private info");
		return -1;
	}

	for (i = 0; i < 512; i++) {
		priv->pcm_data[0][i] = audio->pcm[0][i];
		priv->pcm_data[1][i] = audio->pcm[1][i];
	}

	for (i = 0;i < 256; i++) {
		freqdata[0][i] = audio->freq[0][i];
		freqdata[1][i] = audio->freq[1][i];
	}

	priv->lys.conteur[ZERO]++;
	priv->lys.conteur[ONE]++;

	priv->lys.dt = time_last(priv, TWO, OUI);

	spectre_moyen(priv, freqdata);
	C_E_moyen(priv, freqdata);
	C_dEdt_moyen(priv);
	C_dEdt(priv);

	priv->pixel = ((uint8_t *) video->screenbuffer);

	renderer (priv);

	return 0;
}

void jess_init (JessPrivate *priv)
{
	visual_log_return_if_fail (priv != NULL);

	priv->xres2 = priv->resx / 2;
	priv->yres2 = priv->resy / 2;

	priv->conteur.fullscreen = 0;
	priv->conteur.blur_mode = 1;

	priv->table1 = (uint32_t *) malloc (priv->resx * priv->resy * sizeof (int));
	priv->table2 = (uint32_t *) malloc (priv->resx * priv->resy * sizeof (int));
	priv->table3 = (uint32_t *) malloc (priv->resx * priv->resy * sizeof (int));
	priv->table4 = (uint32_t *) malloc (priv->resx * priv->resy * sizeof (int));

	if (priv->video == 8)
	{
		priv->buffer = (uint8_t *) malloc (priv->resx * priv->resy); 
		memset (priv->buffer, 0, priv->resx * priv->resy);
	}
	else
	{
		priv->buffer = (uint8_t *) malloc (priv->resx * priv->resy * 4);
		memset (priv->buffer, 0, priv->resx * priv->resy * 4);
	}

	create_tables(priv);
}
