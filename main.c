#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glade/glade.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

struct filename {
	char *p;
	int dirlength;
};

static GladeXML *glade_ui;
static GtkWindow *top_window;
static GtkEntry *name_entry;
static GtkTreeView *tree_view;
static GtkListStore *list_store;

static int files_avail;
static int nfiles;
static struct filename *files;

__attribute__((noreturn))
static
void memory_exhausted(void)
{
	fputs("memory exhausted", stderr);
	abort();
}

static
void add_filename(char *p, int dirlength)
{
	struct filename *last;

	if (nfiles == files_avail) {
		int new_avail = files_avail * 2;
		if (!new_avail)
			new_avail = 512;
		files = realloc(files, sizeof(struct filename) * new_avail);
		if (!files)
			memory_exhausted();
		files_avail = new_avail;
	}

	last = files + nfiles++;
	last->p = p;
	last->dirlength = dirlength;
}

#define INIT_BUFSIZE (128*1024)
#define MIN_BUFSIZE_FREE 32768

static
char *input_names(int fd, char **endp)
{
	int bufsize = INIT_BUFSIZE;
	char *buf = malloc(bufsize);
	int filled = 0;

	if (!buf)
		memory_exhausted();

	do {
		int readen = read(fd, buf+filled, bufsize-filled);
		if (readen == 0)
			break;
		else if (readen < 0) {
			if (errno == EINTR)
				continue;
			perror("read_names");
			break;
		}
		filled += readen;
		if (bufsize - filled < MIN_BUFSIZE_FREE) {
			bufsize = filled + MIN_BUFSIZE_FREE;
			buf = realloc(buf, bufsize);
			if (!buf)
				memory_exhausted();
		}
	} while (1);
	if (filled && buf[filled-1])
		filled++;
	buf = realloc(buf, filled);
	if (!buf)
		memory_exhausted();
	buf[filled-1] = 0;
	*endp = buf+filled;
	return buf;
}

static
int filename_compare(const struct filename *a, const struct filename *b)
{
	return strcmp(a->p, b->p);
}

static
void read_filenames(int fd)
{
	char *endp;
	char *buf = input_names(fd, &endp);
	char *p = buf;

	while (p < endp) {
		int dirlength = 0;
		char *start = p;
		char ch;
		while ((ch = *p++))
			if (ch == '/')
				dirlength = p - start;
		add_filename(start, dirlength);
	}

	qsort(files, nfiles, sizeof(struct filename), (int (*)(const void *, const void *))filename_compare);
}

static
void cell_data_func(GtkTreeViewColumn *col,
		    GtkCellRenderer *_renderer,
		    GtkTreeModel *model,
		    GtkTreeIter *iter,
		    gpointer dummy)
{
	GtkCellRendererText *renderer = GTK_CELL_RENDERER_TEXT(_renderer);
	char *text=0;
	gtk_tree_model_get(model, iter, 0, &text, -1);
	if (text) {
		g_object_set(G_OBJECT(renderer),
			     "text", text,
			     0);
	}
	free(text);
}

static
void setup_column(void)
{
	GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *col;
	g_object_set(G_OBJECT(renderer),
		     "ellipsize", PANGO_ELLIPSIZE_START,
		     "alignment", PANGO_ALIGN_RIGHT,
		     0);

	col = gtk_tree_view_column_new_with_attributes("filename", renderer, NULL);

	gtk_tree_view_column_set_cell_data_func(col, renderer, cell_data_func, 0, 0);
	gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);

	gtk_tree_view_append_column(tree_view, col);
}

static
void setup_data(void)
{
	list_store = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_tree_view_set_model(tree_view, GTK_TREE_MODEL(list_store));

	{
		FILE *pipe = popen("find /root/src/altoros/phase1 -type f -print0","r");
		read_filenames(fileno(pipe));
		fclose(pipe);
	}

	{
		int i;
		GtkTreeIter iter;
		for (i=0;i<nfiles;i++) {
			gtk_list_store_append(list_store, &iter);
			gtk_list_store_set(list_store, &iter,
					   0, files[i].p,
					   -1);
		}
	}

	setup_column();
}

static
void exit_program(void)
{
	gtk_main_quit();
}

static
void choice_made(void)
{
	printf("Choice made!\n");
	exit_program();
}

static
gboolean on_top_window_keypress(GtkWidget *_dummy,
				GdkEventKey *event,
				gpointer _dummy2)
{
	if (event->keyval == GDK_Escape) {
		exit_program();
		return TRUE;
	}
	return FALSE;
}

static
void setup_signals(void)
{
	g_signal_connect(top_window, "destroy", G_CALLBACK(exit_program), 0);
	g_signal_connect(top_window, "key-press-event", G_CALLBACK(on_top_window_keypress), 0);
	g_signal_connect(name_entry, "activate", G_CALLBACK(choice_made), 0);
	g_signal_connect(tree_view, "row-activated", G_CALLBACK(choice_made), 0);
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);
	glade_ui = glade_xml_new("filechooser.glade", 0, 0);

	top_window = GTK_WINDOW(glade_xml_get_widget(glade_ui, "top-window"));
	tree_view = GTK_TREE_VIEW(glade_xml_get_widget(glade_ui, "treeview"));
	name_entry = GTK_ENTRY(glade_xml_get_widget(glade_ui, "name-entry"));

	gtk_widget_show_all(GTK_WIDGET(top_window));
	while (gtk_events_pending())
		gtk_main_iteration();

	setup_signals();

	setup_data();

	gtk_main();
	return 0;
}
