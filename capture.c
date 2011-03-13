/* compile with gcc -Wall -o camera-capture -lgphoto2 camera-capture.c
 * This code released into the public domain 21 July 2008
 * 
 * This program does the equivalent of:
 * gphoto2 --shell
 *   > set-config capture=1
 *   > capture-image-and-download
 * compile with gcc -Wall -o camera-capture -lgphoto2 camera-capture.c
 *
 * Taken from: http://credentiality2.blogspot.com/2008/07/linux-libgphoto2-image-capture-from.html 
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gphoto2/gphoto2.h>

#include "samples.h"

struct timeval starttime;

static void errordumper(GPLogLevel level, const char *domain, const char *format,
                 va_list args, void *data) {
	struct timeval tv;

	gettimeofday(&tv,NULL);
	tv.tv_sec -= starttime.tv_sec;
	tv.tv_usec -= starttime.tv_usec;
	if (tv.tv_usec <0) {
		tv.tv_usec += 1000000;
		tv.tv_sec--;
	}
	fprintf(stdout,"%d.%06d: ", (int)tv.tv_sec, (int)tv.tv_usec);
	vfprintf(stdout, format, args);
	fprintf(stdout, "\n");
	fflush (stdout);
}

/* This seems to have no effect on where images go
void set_capturetarget(Camera *camera, GPContext *context) {
	int retval;
	printf("Get root config.\n");
	CameraWidget *rootconfig; // okay, not really
	CameraWidget *actualrootconfig;

	retval = gp_camera_get_config(camera, &rootconfig, context);
	actualrootconfig = rootconfig;
	printf("  Retval: %d\n", retval);

	printf("Get main config.\n");
	CameraWidget *child;
	retval = gp_widget_get_child_by_name(rootconfig, "main", &child);
	printf("  Retval: %d\n", retval);

	printf("Get settings config.\n");
	rootconfig = child;
	retval = gp_widget_get_child_by_name(rootconfig, "settings", &child);
	printf("  Retval: %d\n", retval);

	printf("Get capturetarget.\n");
	rootconfig = child;
	retval = gp_widget_get_child_by_name(rootconfig, "capturetarget", &child);
	printf("  Retval: %d\n", retval);


	CameraWidget *capture = child;

	const char *widgetinfo;
	gp_widget_get_name(capture, &widgetinfo);
	printf("config name: %s\n", widgetinfo );

	const char *widgetlabel;
	gp_widget_get_label(capture, &widgetlabel);
	printf("config label: %s\n", widgetlabel);

	int widgetid;
	gp_widget_get_id(capture, &widgetid);
	printf("config id: %d\n", widgetid);

	CameraWidgetType widgettype;
	gp_widget_get_type(capture, &widgettype);
	printf("config type: %d == %d \n", widgettype, GP_WIDGET_RADIO);


	printf("Set value.\n");

	// capture to ram should be 0, although I think the filename also plays into
	// it
	int one=1;
	retval = gp_widget_set_value(capture, &one);
	printf("  Retval: %d\n", retval);

	printf("Enabling capture to CF.\n");
	retval = gp_camera_set_config(camera, actualrootconfig, context);
	printf("  Retval: %d\n", retval);
}
*/

const char* get_widget_value(CameraWidget *widget)
{
	CameraWidgetType type;
	int ret = gp_widget_get_type (widget, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
	}
	switch (type) {
	        case GP_WIDGET_MENU:
	        case GP_WIDGET_RADIO:
	        case GP_WIDGET_TEXT:
			break;
		default:
			return "?";
	}

	const char *val;
	ret = gp_widget_get_value (widget, &val);
	if (ret < GP_OK) {
		return "??";
	}

	return val;
}


void print_widget_tree(CameraWidget *widget, int level)
{
	int childCount = gp_widget_count_children(widget);
	for (int i=0; i<childCount; i++)
	{
		CameraWidget *child;
		int err = gp_widget_get_child(widget, i, &child);
		if (err != GP_OK)
		{
			printf ("Could not get child %i (%i)\n", i, err);
		}
		else
		{
			const char *name;
			gp_widget_get_name(child, &name);
			for (int j=0; j<level*4; j++) printf(" ");
			printf ("%s - %s\n", name, get_widget_value(child));


			print_widget_tree(child, level+1);
		}
	}
}

static void
capture_to_file(Camera *camera, GPContext *context, char *fn) {
	int fd, retval;
	CameraFile *canonfile;
	CameraFilePath camera_file_path;

	printf("Capturing.\n");

	/* NOP: This gets overridden in the library to /capt0000.jpg */
	strcpy(camera_file_path.folder, "/");
	strcpy(camera_file_path.name, "foo.jpg");

	retval = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &camera_file_path, context);
	printf("  Retval: %d\n", retval);

	printf("Pathname on the camera: %s/%s\n", camera_file_path.folder, camera_file_path.name);

	fd = open(fn, O_CREAT | O_WRONLY, 0644);
	retval = gp_file_new_from_fd(&canonfile, fd);
	printf("  Retval: %d\n", retval);
	retval = gp_camera_file_get(camera, camera_file_path.folder, camera_file_path.name,
		     GP_FILE_TYPE_NORMAL, canonfile, context);
	printf("  Retval: %d\n", retval);

	printf("Deleting.\n");
	retval = gp_camera_file_delete(camera, camera_file_path.folder, camera_file_path.name,
			context);
	printf("  Retval: %d\n", retval);

	gp_file_free(canonfile);
}

int
main(int argc, char **argv) {
	Camera	*camera;
	int	retval;
	GPContext *context = sample_create_context();

	gettimeofday(&starttime,NULL);
	gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
	gp_camera_new(&camera);

	/* When I set GP_LOG_DEBUG instead of GP_LOG_ERROR above, I noticed that the
	 * init function seems to traverse the entire filesystem on the camera.  This
	 * is partly why it takes so long.
	 * (Marcus: the ptp2 driver does this by default currently.)
	 */
	printf("Camera init.  Takes about 10 seconds.\n");
	retval = gp_camera_init(camera, context);
	if (retval != GP_OK) {
		printf("  Retval: %d\n", retval);
		exit (1);
	}

	char* owner;
	retval = get_config_value_string (camera, "ownername", &owner, context);
	if (retval < GP_OK) {
		printf ("Could not query owner.\n");
	}
	else
	{
		printf ("Owner is %s\n", owner);
	}

	CameraWidget *widget;
	gp_camera_get_config (camera, &widget, context);
	print_widget_tree(widget, 0);
	gp_widget_free(widget);

	//canon_enable_capture(camera, TRUE, context);
	/*set_capturetarget(camera, context);*/
	//capture_to_file(camera, context, "foo.cr2");
	gp_camera_exit(camera, context);
	return 0;
}
