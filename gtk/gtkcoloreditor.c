/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* TODO
 * - touch
 * - accessible relations for popups
 * - saving per-application (?)
 * - better popup theming
 */

#include "config.h"

#include "gtkcoloreditorprivate.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcolorplaneprivate.h"
#include "gtkcolorscaleprivate.h"
#include "gtkcolorswatchprivate.h"
#include "gtkcolorutils.h"
#include "gtkgrid.h"
#include "gtkorientable.h"
#include "gtkentry.h"
#include "gtkoverlay.h"
#include "gtkadjustment.h"
#include "gtklabel.h"
#include "gtkspinbutton.h"
#include "gtkintl.h"
#include "gtkstylecontext.h"

#include <math.h>

struct _GtkColorEditorPrivate
{
  GtkWidget *overlay;
  GtkWidget *grid;
  GtkWidget *swatch;
  GtkWidget *entry;
  GtkWidget *h_slider;
  GtkWidget *h_popup;
  GtkWidget *h_entry;
  GtkWidget *a_slider;
  GtkWidget *a_popup;
  GtkWidget *a_entry;
  GtkWidget *sv_plane;
  GtkWidget *sv_popup;
  GtkWidget *s_entry;
  GtkWidget *v_entry;
  GtkWidget *current_popup;
  GtkWidget *popdown_focus;

  GtkAdjustment *h_adj;
  GtkAdjustment *s_adj;
  GtkAdjustment *v_adj;
  GtkAdjustment *a_adj;

  guint text_changed : 1;
  guint use_alpha    : 1;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_USE_ALPHA
};

static void gtk_color_editor_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorEditor, gtk_color_editor, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GtkColorEditor)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_editor_iface_init))

static guint
scale_round (gdouble value, gdouble scale)
{
  value = floor (value * scale + 0.5);
  value = MAX (value, 0);
  value = MIN (value, scale);
  return (guint)value;
}

static void
entry_set_rgba (GtkColorEditor *editor,
                const GdkRGBA  *color)
{
  gchar *text;

  text = g_strdup_printf ("#%02X%02X%02X",
                          scale_round (color->red, 255),
                          scale_round (color->green, 255),
                          scale_round (color->blue, 255));
  gtk_entry_set_text (GTK_ENTRY (editor->priv->entry), text);
  editor->priv->text_changed = FALSE;
  g_free (text);
}

static void
entry_apply (GtkWidget      *entry,
             GtkColorEditor *editor)
{
  GdkRGBA color;
  gchar *text;

  if (!editor->priv->text_changed)
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (editor->priv->entry), 0, -1);
  if (gdk_rgba_parse (&color, text))
    {
      color.alpha = gtk_adjustment_get_value (editor->priv->a_adj);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (editor), &color);
    }

  editor->priv->text_changed = FALSE;

  g_free (text);
}

static gboolean
entry_focus_out (GtkWidget      *entry,
                 GdkEventFocus  *event,
                 GtkColorEditor *editor)
{
  entry_apply (entry, editor);
  return FALSE;
}

static void
entry_text_changed (GtkWidget      *entry,
                    GParamSpec     *pspec,
                    GtkColorEditor *editor)
{
  editor->priv->text_changed = TRUE;
}

static void
hsv_changed (GtkColorEditor *editor)
{
  GdkRGBA color;
  gdouble h, s, v, a;

  h = gtk_adjustment_get_value (editor->priv->h_adj);
  s = gtk_adjustment_get_value (editor->priv->s_adj);
  v = gtk_adjustment_get_value (editor->priv->v_adj);
  a = gtk_adjustment_get_value (editor->priv->a_adj);

  gtk_hsv_to_rgb (h, s, v, &color.red, &color.green, &color.blue);
  color.alpha = a;

  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (editor->priv->swatch), &color);
  gtk_color_scale_set_rgba (GTK_COLOR_SCALE (editor->priv->a_slider), &color);
  entry_set_rgba (editor, &color);

  g_object_notify (G_OBJECT (editor), "rgba");
}

static void
dismiss_current_popup (GtkColorEditor *editor)
{
  if (editor->priv->current_popup)
    {
      gtk_widget_hide (editor->priv->current_popup);
      editor->priv->current_popup = NULL;
      if (editor->priv->popdown_focus)
        {
          gtk_widget_grab_focus (editor->priv->popdown_focus);
          editor->priv->popdown_focus = NULL;
        }
    }
}

static void
popup_edit (GtkWidget      *widget,
            GtkColorEditor *editor)
{
  GtkWidget *popup = NULL;
  GtkWidget *toplevel;
  GtkWidget *focus;

  if (widget == editor->priv->sv_plane)
    {
      popup = editor->priv->sv_popup;
      focus = editor->priv->s_entry;
    }
  else if (widget == editor->priv->h_slider)
    {
      popup = editor->priv->h_popup;
      focus = editor->priv->h_entry;
    }
  else if (widget == editor->priv->a_slider)
    {
      popup = editor->priv->a_popup;
      focus = editor->priv->a_entry;
    }

  if (popup)
    {
      dismiss_current_popup (editor);
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editor));
      editor->priv->popdown_focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
      editor->priv->current_popup = popup;
      gtk_widget_show (popup);
      gtk_widget_grab_focus (focus);
    }
}

static gboolean
popup_key_press (GtkWidget      *popup,
                 GdkEventKey    *event,
                 GtkColorEditor *editor)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      dismiss_current_popup (editor);
      return TRUE;
    }

  return FALSE;
}

static gboolean
get_child_position (GtkOverlay     *overlay,
                    GtkWidget      *widget,
                    GtkAllocation  *allocation,
                    GtkColorEditor *editor)
{
  GtkRequisition req;
  GtkAllocation alloc;
  gint s, e;

  gtk_widget_get_preferred_size (widget, &req, NULL);

  allocation->width = req.width;
  allocation->height = req.height;

  if (widget == editor->priv->sv_popup)
    {
      if (gtk_widget_get_direction (GTK_WIDGET (overlay)) == GTK_TEXT_DIR_RTL)
        allocation->x = 0;
      else
        allocation->x = gtk_widget_get_allocated_width (GTK_WIDGET (overlay)) - req.width;
      allocation->y = req.height / 3;
    }
  else if (widget == editor->priv->h_popup)
    {
      gtk_widget_get_allocation (editor->priv->h_slider, &alloc);
      gtk_range_get_slider_range (GTK_RANGE (editor->priv->h_slider), &s, &e);

      if (gtk_widget_get_direction (GTK_WIDGET (overlay)) == GTK_TEXT_DIR_RTL)
        gtk_widget_translate_coordinates (editor->priv->h_slider,
                                          gtk_widget_get_parent (editor->priv->grid),
                                          - req.width, (s + e - req.height) / 2,
                                          &allocation->x, &allocation->y);
      else
        gtk_widget_translate_coordinates (editor->priv->h_slider,
                                          gtk_widget_get_parent (editor->priv->grid),
                                          alloc.width, (s + e - req.height) / 2,
                                          &allocation->x, &allocation->y);
    }
  else if (widget == editor->priv->a_popup)
    {
      gtk_widget_get_allocation (editor->priv->a_slider, &alloc);
      gtk_range_get_slider_range (GTK_RANGE (editor->priv->a_slider), &s, &e);

      gtk_widget_translate_coordinates (editor->priv->a_slider,
                                        gtk_widget_get_parent (editor->priv->grid),
                                        (s + e - req.width) / 2, - req.height,
                                        &allocation->x, &allocation->y);
    }
  else
    return FALSE;

  allocation->x = CLAMP (allocation->x, 0, gtk_widget_get_allocated_width (GTK_WIDGET (overlay)) - req.width);
  allocation->y = CLAMP (allocation->y, 0, gtk_widget_get_allocated_height (GTK_WIDGET (overlay)) - req.height);

  return TRUE;
}

static void
value_changed (GtkAdjustment *a,
               GtkAdjustment *as)
{
  gdouble scale;

  scale = gtk_adjustment_get_upper (as) / gtk_adjustment_get_upper (a);
  g_signal_handlers_block_by_func (as, value_changed, a);
  gtk_adjustment_set_value (as, gtk_adjustment_get_value (a) * scale);
  g_signal_handlers_unblock_by_func (as, value_changed, a);
}

static GtkAdjustment *
scaled_adjustment (GtkAdjustment *a,
                   gdouble        scale)
{
  GtkAdjustment *as;

  as = gtk_adjustment_new (gtk_adjustment_get_value (a) * scale,
                           gtk_adjustment_get_lower (a) * scale,
                           gtk_adjustment_get_upper (a) * scale,
                           gtk_adjustment_get_step_increment (a) * scale,
                           gtk_adjustment_get_page_increment (a) * scale,
                           gtk_adjustment_get_page_size (a) * scale);

  g_signal_connect (a, "value-changed", G_CALLBACK (value_changed), as);
  g_signal_connect (as, "value-changed", G_CALLBACK (value_changed), a);

  return as;
}

static gboolean
popup_draw (GtkWidget      *popup,
            cairo_t        *cr,
            GtkColorEditor *editor)
{
  GtkStyleContext *context;
  gint width, height;

  context = gtk_widget_get_style_context (popup);
  width = gtk_widget_get_allocated_width (popup);
  height = gtk_widget_get_allocated_height (popup);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  return FALSE;
}

static void
gtk_color_editor_init (GtkColorEditor *editor)
{
  GdkRGBA transparent = { 0, 0, 0, 0 };

  editor->priv = gtk_color_editor_get_instance_private (editor);
  editor->priv->use_alpha = TRUE;

  g_type_ensure (GTK_TYPE_COLOR_SCALE);
  g_type_ensure (GTK_TYPE_COLOR_PLANE);
  g_type_ensure (GTK_TYPE_COLOR_SWATCH);
  gtk_widget_init_template (GTK_WIDGET (editor));

  /* Some post processing is needed in code to set this up */
  gtk_widget_set_events (editor->priv->swatch,
			 gtk_widget_get_events (editor->priv->swatch)
                                 & ~(GDK_BUTTON_PRESS_MASK
                                     | GDK_BUTTON_RELEASE_MASK
                                     | GDK_KEY_PRESS_MASK
                                     | GDK_KEY_RELEASE_MASK));

  if (gtk_widget_get_direction (editor->priv->h_slider) == GTK_TEXT_DIR_RTL)
    gtk_style_context_add_class (gtk_widget_get_style_context (editor->priv->h_slider),
                                 GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE);
  else
    gtk_style_context_add_class (gtk_widget_get_style_context (editor->priv->h_slider),
                                 GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW);

  gtk_widget_override_background_color (editor->priv->overlay, 0, &transparent);

  /* Create the scaled popup adjustments manually here because connecting user data is not
   * supported by template GtkBuilder xml (it would be possible to set this up in the xml
   * but require 4 separate callbacks and would be rather ugly).
   */
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->priv->h_entry), scaled_adjustment (editor->priv->h_adj, 100));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->priv->s_entry), scaled_adjustment (editor->priv->s_adj, 100));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->priv->v_entry), scaled_adjustment (editor->priv->v_adj, 100));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->priv->a_entry), scaled_adjustment (editor->priv->a_adj, 100));

  /* This can be setup in the .ui file, but requires work in Glade otherwise it cannot be edited there */
  gtk_overlay_add_overlay (GTK_OVERLAY (editor->priv->overlay), editor->priv->sv_popup);
  gtk_overlay_add_overlay (GTK_OVERLAY (editor->priv->overlay), editor->priv->h_popup);
  gtk_overlay_add_overlay (GTK_OVERLAY (editor->priv->overlay), editor->priv->a_popup);
}

static void
gtk_color_editor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkColorEditor *ce = GTK_COLOR_EDITOR (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      {
        GdkRGBA color;
        gtk_color_chooser_get_rgba (cc, &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, gtk_widget_get_visible (ce->priv->a_slider));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_set_use_alpha (GtkColorEditor *editor,
                                gboolean        use_alpha)
{
  if (editor->priv->use_alpha != use_alpha)
    {
      editor->priv->use_alpha = use_alpha;
      gtk_widget_set_visible (editor->priv->a_slider, use_alpha);
      gtk_color_swatch_set_use_alpha (GTK_COLOR_SWATCH (editor->priv->swatch), use_alpha);
    }
}

static void
gtk_color_editor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorEditor *ce = GTK_COLOR_EDITOR (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (cc, g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      gtk_color_editor_set_use_alpha (ce, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_class_init (GtkColorEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_color_editor_get_property;
  object_class->set_property = gtk_color_editor_set_property;

  g_object_class_override_property (object_class, PROP_RGBA, "rgba");
  g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/gtkcoloreditor.ui");

  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, overlay);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, grid);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, swatch);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, entry);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, h_slider);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, h_popup);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, h_entry);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, a_slider);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, a_popup);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, a_entry);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, sv_plane);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, sv_popup);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, s_entry);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, v_entry);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, h_adj);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, s_adj);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, v_adj);
  gtk_widget_class_bind_child (widget_class, GtkColorEditorPrivate, a_adj);

  gtk_widget_class_bind_callback (widget_class, hsv_changed);
  gtk_widget_class_bind_callback (widget_class, popup_draw);
  gtk_widget_class_bind_callback (widget_class, popup_key_press);
  gtk_widget_class_bind_callback (widget_class, dismiss_current_popup);
  gtk_widget_class_bind_callback (widget_class, get_child_position);
  gtk_widget_class_bind_callback (widget_class, entry_text_changed);
  gtk_widget_class_bind_callback (widget_class, entry_apply);
  gtk_widget_class_bind_callback (widget_class, entry_focus_out);
  gtk_widget_class_bind_callback (widget_class, popup_edit);
}

static void
gtk_color_editor_get_rgba (GtkColorChooser *chooser,
                           GdkRGBA         *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);
  gdouble h, s, v;

  h = gtk_adjustment_get_value (editor->priv->h_adj);
  s = gtk_adjustment_get_value (editor->priv->s_adj);
  v = gtk_adjustment_get_value (editor->priv->v_adj);
  gtk_hsv_to_rgb (h, s, v, &color->red, &color->green, &color->blue);
  color->alpha = gtk_adjustment_get_value (editor->priv->a_adj);
}

static void
gtk_color_editor_set_rgba (GtkColorChooser *chooser,
                           const GdkRGBA   *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);
  gdouble h, s, v;

  gtk_rgb_to_hsv (color->red, color->green, color->blue, &h, &s, &v);

  gtk_adjustment_set_value (editor->priv->h_adj, h);
  gtk_adjustment_set_value (editor->priv->s_adj, s);
  gtk_adjustment_set_value (editor->priv->v_adj, v);
  gtk_adjustment_set_value (editor->priv->a_adj, color->alpha);

  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (editor->priv->swatch), color);
  gtk_color_scale_set_rgba (GTK_COLOR_SCALE (editor->priv->a_slider), color);
  entry_set_rgba (editor, color);

  g_object_notify (G_OBJECT (editor), "rgba");
}

static void
gtk_color_editor_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_editor_get_rgba;
  iface->set_rgba = gtk_color_editor_set_rgba;
}

GtkWidget *
gtk_color_editor_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_EDITOR, NULL);
}
