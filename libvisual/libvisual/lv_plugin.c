#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>

#include "lvconfig.h"
#include "lv_libvisual.h"
#include "lv_plugin.h"
#include "lv_log.h"
#include "lv_mem.h"

extern VisList *__lv_plugins;

static void ref_list_destroy (void *ref);

static int plugin_add_dir_to_list (VisList *list, const char *dir);

static void ref_list_destroy (void *data)
{
	VisPluginRef *ref;

	if (data == NULL)
		return;

	ref = (VisPluginRef *) data;

	visual_plugin_info_free (ref->info);
	visual_plugin_ref_free (ref);
}

/**
 * @defgroup VisPlugin VisPlugin
 * @{
 */

/**
 * Creates a new VisPluginInfo structure.
 *
 * @return A newly allocated VisPluginInfo
 */
VisPluginInfo *visual_plugin_info_new ()
{
	VisPluginInfo *pluginfo;

	pluginfo = visual_mem_new0 (VisPluginInfo, 1);

	return pluginfo;
}

/**
 * Frees the VisPluginInfo. This frees the VisPluginInfo data structure.
 *
 * @param pluginfo Pointer to the VisPluginInfo that needs to be freed.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_info_free (VisPluginInfo *pluginfo)
{
	visual_log_return_val_if_fail (pluginfo != NULL, -VISUAL_ERROR_PLUGIN_INFO_NULL);

	if (pluginfo->plugname != NULL)
		visual_mem_free (pluginfo->plugname);

	if (pluginfo->name != NULL)
		visual_mem_free (pluginfo->name);

	if (pluginfo->author != NULL)
		visual_mem_free (pluginfo->author);

	if (pluginfo->version != NULL)
		visual_mem_free (pluginfo->version);

	if (pluginfo->about != NULL)
		visual_mem_free (pluginfo->about);

	if (pluginfo->help != NULL)
		visual_mem_free (pluginfo->help);

	return visual_mem_free (pluginfo);
}

/**
 * Copies data from one VisPluginInfo to another, this does not copy everything
 * but only things that are needed in the local copy for the plugin registry.
 *
 * @param dest Pointer to the destination VisPluginInfo in which some data is copied.
 * @param src Pointer to the source VisPluginInfo from which some data is copied.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_info_copy (VisPluginInfo *dest, const VisPluginInfo *src)
{
	visual_log_return_val_if_fail (dest != NULL, -VISUAL_ERROR_PLUGIN_INFO_NULL);
	visual_log_return_val_if_fail (src != NULL, -VISUAL_ERROR_PLUGIN_INFO_NULL);

	memcpy (dest, src, sizeof (VisPluginInfo));

	dest->plugname = strdup (src->plugname);
	dest->name = strdup (src->name);
	dest->author = strdup (src->author);
	dest->version = strdup (src->version);
	dest->about = strdup (src->about);
	dest->help = strdup (src->help);

	return VISUAL_OK;
}

/**
 * Pumps the queued events into the plugin it's event handler if it has one.
 *
 * @param plugin Pointer to a VisPluginData of which the events need to be pumped into
 *	the handler.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_events_pump (VisPluginData *plugin)
{
	visual_log_return_val_if_fail (plugin != NULL, -VISUAL_ERROR_PLUGIN_NULL);

	if (plugin->info->events != NULL) {
		plugin->info->events (plugin, &plugin->eventqueue);

		return VISUAL_OK;
	}

	return -VISUAL_ERROR_PLUGIN_NO_EVENT_HANDLER;
}

/**
 * Gives the event queue from a VisPluginData. This queue needs to be used
 * when you want to send events to the plugin.
 *
 * @see visual_plugin_events_pump
 *
 * @param plugin Pointer to the VisPluginData from which we want the queue.
 *
 * @return A pointer to the requested VisEventQueue or NULL on error.
 */
VisEventQueue *visual_plugin_get_eventqueue (VisPluginData *plugin)
{
	visual_log_return_val_if_fail (plugin != NULL, NULL);

	return &plugin->eventqueue;
}

/**
 * Sets a VisUIWidget as top user interface widget for the plugin. When a VisUI
 * tree is requested by a client, to render a configuration userinterface, this
 * VisUIWidget is used as top widget.
 *
 * @param plugin Pointer to the VisPluginData to which we set the VisUIWidget as top widget.
 * @param widget Pointer to the VisUIWidget that we use as top widget for the user interface.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_set_userinterface (VisPluginData *plugin, VisUIWidget *widget)
{
	visual_log_return_val_if_fail (plugin != NULL, -VISUAL_ERROR_PLUGIN_NULL);

	plugin->userinterface = widget;

	return VISUAL_OK;
}

/**
 * Retrieves the VisUI top widget for the plugin.
 *
 * @param plugin Pointer to the VisPluginData of which we request the VisUIWidget that serves as top widget.
 *
 * @return Pointer to the VisUIWidget that serves as top widget, possibly NULL.
 */
VisUIWidget *visual_plugin_get_userinterface (VisPluginData *plugin)
{
	visual_log_return_val_if_fail (plugin != NULL, NULL);

	return plugin->userinterface;
}

/**
 * Gives the VisPluginInfo related to a VisPluginData.
 *
 * @param plugin The VisPluginData of which the VisPluginInfo is requested.
 *
 * @return The VisPluginInfo within the VisPluginData, or NULL on error.
 */
const VisPluginInfo *visual_plugin_get_info (const VisPluginData *plugin)
{
	visual_log_return_val_if_fail (plugin != NULL, NULL);

	return plugin->info;
}

/**
 * Gives the VisParamContainer related to a VisPluginData.
 *
 * @param plugin The VisPluginData of which the VisParamContainer is requested.
 *
 * @return The VisParamContainer within the VisPluginData, or NULL on error.
 */
VisParamContainer *visual_plugin_get_params (VisPluginData *plugin)
{
	visual_log_return_val_if_fail (plugin != NULL, NULL);

	return &plugin->params;
}

/**
 * Gives the VisRandomContext related to a VisPluginData.
 *
 * @param plugin The VisPluginData of which the VisRandomContext is requested.
 *
 * @return The VisRandomContext within the VisPluginDAta, or NULL on error.
 */
VisRandomContext *visual_plugin_get_random_context (VisPluginData *plugin)
{
	visual_log_return_val_if_fail (plugin != NULL, NULL);

	return &plugin->random;
}

/**
 * Retrieves the plugin specific part of a plugin.
 *
 * @see VISUAL_PLUGIN_ACTOR
 * @see VISUAL_PLUGIN_INPUT
 * @see VISUAL_PLUGIN_MORPH
 * 
 * @param plugin The pointer to the VisPluginData from which we want the plugin specific part.
 *
 * @return Void * pointing to the plugin specific part which can be cast.
 */
void *visual_plugin_get_specific (VisPluginData *plugin)
{
	const VisPluginInfo *pluginfo;

	visual_log_return_val_if_fail (plugin != NULL, NULL);

	pluginfo = visual_plugin_get_info (plugin);
	visual_log_return_val_if_fail (pluginfo != NULL, NULL);
	
	return pluginfo->plugin;
}

/**
 * Creates a new VisPluginRef structure.
 *
 * The VisPluginRef contains data for the plugin loader.
 *
 * @return Newly allocated VisPluginRef.
 */
VisPluginRef *visual_plugin_ref_new ()
{
	return (visual_mem_new0 (VisPluginRef, 1));
}

/**
 * Frees the VisPluginRef. This frees the VisPluginRef data structure.
 *
 * @param ref Pointer to the VisPluginRef that needs to be freed.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_ref_free (VisPluginRef *ref)
{
	visual_log_return_val_if_fail (ref != NULL, -VISUAL_ERROR_PLUGIN_REF_NULL);

	if (ref->file != NULL)
		visual_mem_free (ref->file);

	if (ref->usecount > 0)
		visual_log (VISUAL_LOG_CRITICAL, "A plugin reference with %d instances has been destroyed.", ref->usecount);
	
	visual_mem_free (ref);

	return VISUAL_OK;
}

/**
 * Destroys a VisList of plugin references. This frees all the
 * references in a list and the list itself. This is used internally
 * to destroy the plugin registry.
 *
 * @param list The list of VisPluginRefs that need to be destroyed.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_ref_list_destroy (VisList *list)
{
	visual_log_return_val_if_fail (list != NULL, -VISUAL_ERROR_LIST_NULL);

	return visual_list_destroy (list, ref_list_destroy);
}

/**
 * Creates a new VisPluginData structure.
 *
 * @return A newly allocated VisPluginData.
 */
VisPluginData *visual_plugin_new ()
{
	return (visual_mem_new0 (VisPluginData, 1));
}

/**
 * Frees the VisPluginData. This frees the VisPluginData data structure.
 *
 * @param plugin Pointer to the VisPluginData that needs to be freed.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_free (VisPluginData *plugin)
{
	visual_log_return_val_if_fail (plugin != NULL, -VISUAL_ERROR_PLUGIN_NULL);

	return visual_mem_free (plugin);
}

/**
 * Gives a VisList that contains references to all the plugins in the registry.
 *
 * @see VisPluginRef
 * 
 * @return VisList of references to all the libvisual plugins.
 */
const VisList *visual_plugin_get_registry ()
{
	return __lv_plugins;
}

/**
 * Gives a newly allocated VisList with references for one plugin type.
 *
 * @see VisPluginRef
 *
 * @param pluglist Pointer to the VisList that contains the plugin registry.
 * @param type The plugin type that is filtered for.
 *
 * @return Newly allocated VisList that is a filtered version of the plugin registry.
 */
VisList *visual_plugin_registry_filter (const VisList *pluglist, VisPluginType type)
{
	VisList *list;
	VisListEntry *entry = NULL;
	VisPluginRef *ref;

	visual_log_return_val_if_fail (pluglist != NULL, NULL);

	list = visual_list_new ();

	if (list == NULL) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot create a new list");
		return NULL;
	}

	while ((ref = visual_list_next (pluglist, &entry)) != NULL) {
		if (ref->info->type == type)
			visual_list_add (list, ref);
	}

	return list;
}

/**
 * Get the next plugin based on it's name.
 *
 * @see visual_plugin_registry_filter
 * 
 * @param list Pointer to the VisList containing the plugins. Adviced is to filter
 *	this list first using visual_plugin_registry_filter.
 * @param name Name of a plugin entry of which we want the next entry or NULL to get
 * 	the first entry.
 *
 * @return The name of the next plugin or NULL on error.
 */
const char *visual_plugin_get_next_by_name (const VisList *list, const char *name)
{
	VisListEntry *entry = NULL;
	VisPluginRef *ref;
	int tagged = FALSE;

	visual_log_return_val_if_fail (list != NULL, NULL);

	while ((ref = visual_list_next (list, &entry)) != NULL) {
		if (name == NULL)
			return ref->info->plugname;

		if (tagged == TRUE)
			return ref->info->plugname;

		if (strcmp (name, ref->info->plugname) == 0)
			tagged = TRUE;
	}

	return NULL;
}

/**
 * Get the previous plugin based on it's name.
 *
 * @see visual_plugin_registry_filter
 * 
 * @param list Pointer to the VisList containing the plugins. Adviced is to filter
 *	this list first using visual_plugin_registry_filter.
 * @param name Name of a plugin entry of which we want the previous entry or NULL to get
 * 	the last entry.
 *
 * @return The name of the next plugin or NULL on error.
 */
const char *visual_plugin_get_prev_by_name (const VisList *list, const char *name)
{
	VisListEntry *entry = NULL;
	VisPluginRef *ref, *pref = NULL;
	
	visual_log_return_val_if_fail (list != NULL, NULL);

	if (name == NULL) {
		ref = visual_list_get (list, visual_list_count (list) - 1);
		
		if (ref == NULL)
			return NULL;
		
		return ref->info->plugname;
	}

	while ((ref = visual_list_next (list, &entry)) != NULL) {
		if (strcmp (name, ref->info->plugname) == 0) {
			if (pref != NULL)
				return pref->info->plugname;
			else
				return NULL;
		}

		pref = ref;
	}

	return NULL;
}

static int plugin_add_dir_to_list (VisList *list, const char *dir)
{
	VisPluginRef **ref;
	char temp[1024];
	struct dirent **namelist;
	int i, j, n, len;
	int cnt = 0;

	n = scandir (dir, &namelist, 0, alphasort);

	if (n < 0)
		return -1;

	/* Free the . and .. entries */
	visual_mem_free (namelist[0]);
	visual_mem_free (namelist[1]);

	for (i = 2; i < n; i++) {
		ref = NULL;

		snprintf (temp, 1023, "%s/%s", dir, namelist[i]->d_name);

		len = strlen (temp);
		if (len > 3 && (strncmp (&temp[len - 3], ".so", 3)) == 0)
			ref = visual_plugin_get_references (temp, &cnt);

		if (ref != NULL) {
			for (j = 0; j < cnt; j++) 
				visual_list_add (list, ref[j]);
		}

		if (ref != NULL)
			visual_mem_free (ref);

		visual_mem_free (namelist[i]);
	}

	visual_mem_free (namelist);

	return 0;
}

/**
 * Private function to unload a plugin. After calling this function the
 * given argument is no longer usable.
 *
 * @param plugin Pointer to the VisPluginData that needs to be unloaded.
 *
 * @return 0 on succes -1 on error.
 */
int visual_plugin_unload (VisPluginData *plugin)
{
	VisPluginRef *ref;

	visual_log_return_val_if_fail (plugin != NULL, -VISUAL_ERROR_PLUGIN_NULL);

	ref = plugin->ref;

	/* Not loaded */
	if (plugin->handle == NULL) {
		visual_mem_free (plugin);

		visual_log (VISUAL_LOG_CRITICAL, "Tried unloading a plugin that never has been loaded.");

		return -VISUAL_ERROR_PLUGIN_HANDLE_NULL;
	}
	
	if (plugin->realized == TRUE)
		plugin->info->cleanup (plugin);

	dlclose (plugin->handle);

	visual_mem_free (plugin);
	
	visual_log_return_val_if_fail (ref != NULL, -VISUAL_ERROR_PLUGIN_REF_NULL);
	
	if (ref->usecount > 0)
		ref->usecount--;

	return VISUAL_OK;
}

/**
 * Private function to load a plugin.
 *
 * @param ref Pointer to the VisPluginRef containing information about
 *	the plugin that needs to be loaded.
 *
 * @return A newly created and loaded VisPluginData.
 */
VisPluginData *visual_plugin_load (VisPluginRef *ref)
{
	VisPluginData *plugin;
	VisTime time_;
	const VisPluginInfo *pluginfo;
	plugin_get_info_func_t get_plugin_info;
	void *handle;
	int cnt;

	visual_log_return_val_if_fail (ref != NULL, NULL);
	visual_log_return_val_if_fail (ref->info != NULL, NULL);

	/* Check if this plugin is reentrant */
	if (ref->usecount > 0 && (ref->info->flags & VISUAL_PLUGIN_FLAG_NOT_REENTRANT)) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot load plugin %s, the plugin is already loaded and is not reentrant.",
				ref->info->plugname);

		return NULL;
	}

	handle = dlopen (ref->file, RTLD_LAZY);

	if (handle == NULL) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot load plugin: %s", dlerror ());

		return NULL;
	}
	
	get_plugin_info = (plugin_get_info_func_t) dlsym (handle, "get_plugin_info");
	
	if (get_plugin_info == NULL) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot initialize plugin: %s", dlerror ());

		dlclose (handle);

		return NULL;
	}

	pluginfo = get_plugin_info (&cnt);

	if (pluginfo == NULL) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot get plugin info while loading.");

		dlclose (handle);
		
		return NULL;
	}

	plugin = visual_mem_new0 (VisPluginData, 1);
	plugin->ref = ref;
	plugin->info = &pluginfo[ref->index];

	ref->usecount++;
	plugin->realized = FALSE;
	plugin->handle = handle;

	/* Now the plugin is set up and ready to be realized, also random seed it's random context */
	visual_time_get (&time_);
	visual_random_context_set_seed (&plugin->random, time_.tv_usec);

	return plugin;
}

/**
 * Private function to realize the plugin. This initializes the plugin.
 *
 * @param plugin Pointer to the VisPluginData that needs to be realized.
 * 
 * @return 0 on succes -1 on error.
 */
int visual_plugin_realize (VisPluginData *plugin)
{
	VisParamContainer *paramcontainer;

	visual_log_return_val_if_fail (plugin != NULL, -VISUAL_ERROR_PLUGIN_NULL);

	if (plugin->realized == TRUE)
		return -VISUAL_ERROR_PLUGIN_ALREADY_REALIZED;

	paramcontainer = visual_plugin_get_params (plugin);
	visual_param_container_set_eventqueue (paramcontainer, &plugin->eventqueue);

	plugin->info->init (plugin);
	plugin->realized = TRUE;

	return VISUAL_OK;
}

/**
 * Private function to create VisPluginRefs from plugins.
 *
 * @param pluginpath The full path and filename to the plugin of which a reference
 *	needs to be obtained.
 * @param count Int pointer that will contain the number of VisPluginRefs returned.
 *
 * @return The optionally newly allocated VisPluginRefs for the plugin.
 */
VisPluginRef **visual_plugin_get_references (const char *pluginpath, int *count)
{
	VisPluginRef **ref;
	const VisPluginInfo *plug_info;
	VisPluginInfo *dup_info;
	const char *plug_name;
	plugin_get_info_func_t get_plugin_info;
	void *handle;
	int cnt = 1, i;

	visual_log_return_val_if_fail (pluginpath != NULL, NULL);

	handle = dlopen (pluginpath, RTLD_LAZY);
	
	if (handle == NULL) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot load plugin: %s", dlerror ());

		return NULL;
	}

	get_plugin_info = (plugin_get_info_func_t) dlsym (handle, "get_plugin_info");

	if (get_plugin_info == NULL) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot initialize plugin: %s", dlerror ());

		dlclose (handle);

		return NULL;
	}

	plug_info = get_plugin_info (&cnt);

	if (plug_info == NULL) {
		visual_log (VISUAL_LOG_CRITICAL, "Cannot get plugin info");

		dlclose (handle);
		
		return NULL;
	}
	
	/* Check for API and struct size */
	if (plug_info[0].struct_size != sizeof (VisPluginInfo) ||
			plug_info[0].api_version != VISUAL_PLUGIN_API_VERSION) {

		visual_log (VISUAL_LOG_CRITICAL, "Plugin %s is not compatible with version %s of libvisual",
				pluginpath, visual_get_version ());

		dlclose (handle);

		return NULL;
	}

	ref = visual_mem_new0 (VisPluginRef *, cnt);
	
	for (i = 0; i < cnt; i++) {
		ref[i] = visual_mem_new0 (VisPluginRef, 1);

		dup_info = visual_plugin_info_new ();
		visual_plugin_info_copy (dup_info, (VisPluginInfo *) &plug_info[i]);
		
		ref[i]->index = i;
		ref[i]->info = dup_info;
		ref[i]->file = strdup (pluginpath);
	}

	dlclose (handle);
	
	*count = cnt;	

	return ref;
}

/**
 * Private function to create the complete plugin registry from a set of paths.
 *
 * @param paths A pointer list to a set of paths.
 *
 * @return A newly allocated VisList containing the plugin registry for the set of paths.
 */
VisList *visual_plugin_get_list (const char **paths)
{
	VisList *list = visual_list_new();
	int i = 0;

	while (paths[i] != NULL) {
		if (plugin_add_dir_to_list (list, paths[i++]) < 0) {
			visual_list_destroy (list, NULL);
			return NULL;
		}
	}
	
	return list;
}

/**
 * Private function to find a plugin in a plugin registry.
 *
 * @param list Pointer to a VisList containing VisPluginRefs in which
 *	the search is done.
 * @param name The name of the plugin we're looking for.
 *
 * @return The VisPluginRef for the plugin if found, or NULL when not found.
 */
VisPluginRef *visual_plugin_find (const VisList *list, const char *name)
{
	VisListEntry *entry = NULL;
	VisPluginRef *ref;

	while ((ref = visual_list_next (list, &entry)) != NULL) {

		if (ref->info->plugname == NULL)
			continue;

		if (strcmp (name, ref->info->plugname) == 0)
			return ref;
	}

	return NULL;
}

/**
 * Gives the VISUAL_PLUGIN_API_VERSION value for which the library is compiled.
 * This can be used to check against for API/ABI compatibility check.
 *
 * @return The VISUAL_PLUGIN_API_VERSION define value.
 */
int visual_plugin_get_api_version ()
{
	return VISUAL_PLUGIN_API_VERSION;
}

/**
 * Retrieves the VisSongInfo from a VisActorPlugin.
 *
 * @param actplugin Pointer to the VisActorPlugin from which the VisSongInfo is requested.
 *
 * @return The requested VisSongInfo or NULL on error.
 */
VisSongInfo *visual_plugin_actor_get_songinfo (VisActorPlugin *actplugin)
{
	visual_log_return_val_if_fail (actplugin != NULL, NULL);

	return &actplugin->songinfo;
}

/**
 * @}
 */

