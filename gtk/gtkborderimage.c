/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Cosimo Cecchi <cosimoc@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <cairo-gobject.h>

#include <math.h>

#include "gtkborderimageprivate.h"
#include "gtkstylepropertiesprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

G_DEFINE_BOXED_TYPE (GtkBorderImage, _gtk_border_image,
                     _gtk_border_image_ref, _gtk_border_image_unref)

enum {
  BORDER_LEFT,
  BORDER_MIDDLE,
  BORDER_RIGHT,
  BORDER_LAST,
  BORDER_TOP = BORDER_LEFT,
  BORDER_BOTTOM = BORDER_RIGHT
};

enum {
  SIDE_TOP,
  SIDE_RIGHT,
  SIDE_BOTTOM,
  SIDE_LEFT
};

struct _GtkBorderImage {
  GtkCssImage *source;

  GtkBorder slice;
  GtkBorder *width;
  GtkCssBorderImageRepeat repeat;

  gint ref_count;
};

GtkBorderImage *
_gtk_border_image_new (GtkCssImage             *source,
                       GtkBorder               *slice,
                       GtkBorder               *width,
                       GtkCssBorderImageRepeat *repeat)
{
  GtkBorderImage *image;

  image = g_slice_new0 (GtkBorderImage);
  image->ref_count = 1;

  image->source = g_object_ref (source);

  if (slice != NULL)
    image->slice = *slice;

  if (width != NULL)
    image->width = gtk_border_copy (width);

  if (repeat != NULL)
    image->repeat = *repeat;

  return image;
}

GtkBorderImage *
_gtk_border_image_ref (GtkBorderImage *image)
{
  g_return_val_if_fail (image != NULL, NULL);

  image->ref_count++;

  return image;
}

void
_gtk_border_image_unref (GtkBorderImage *image)
{
  g_return_if_fail (image != NULL);

  image->ref_count--;

  if (image->ref_count == 0)
    {
      g_object_unref (image->source);

      if (image->width != NULL)
        gtk_border_free (image->width);

      g_slice_free (GtkBorderImage, image);
    }
}

GParameter *
_gtk_border_image_unpack (const GValue *value,
                          guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GtkBorderImage *image = g_value_get_boxed (value);

  parameter[0].name = "border-image-source";
  g_value_init (&parameter[0].value, GTK_TYPE_CSS_IMAGE);

  parameter[1].name = "border-image-slice";
  g_value_init (&parameter[1].value, GTK_TYPE_BORDER);

  parameter[2].name = "border-image-repeat";
  g_value_init (&parameter[2].value, GTK_TYPE_CSS_BORDER_IMAGE_REPEAT);

  parameter[3].name = "border-image-width";
  g_value_init (&parameter[3].value, GTK_TYPE_BORDER);

  if (image != NULL)
    {
      g_value_set_object (&parameter[0].value, image->source);
      g_value_set_boxed (&parameter[1].value, &image->slice);
      g_value_set_boxed (&parameter[2].value, &image->repeat);
      g_value_set_boxed (&parameter[3].value, image->width);
    }

  *n_params = 4;
  return parameter;
}

void
_gtk_border_image_pack (GValue             *value,
                        GtkStyleProperties *props,
                        GtkStateFlags       state)
{
  GtkBorderImage *image;
  GtkBorder *slice, *width;
  GtkCssBorderImageRepeat *repeat;
  GtkCssImage *source;
  const GValue *val;

  val = _gtk_style_properties_peek_property (props,
                                             GTK_CSS_STYLE_PROPERTY (_gtk_style_property_lookup ("border-image-source")),
                                             state);
  source = g_value_get_object (val);
  if (source == NULL)
    return;

  gtk_style_properties_get (props, state,
			    "border-image-slice", &slice,
			    "border-image-repeat", &repeat,
			    "border-image-width", &width,
			    NULL);

  image = _gtk_border_image_new (source, slice, width, repeat);
  g_value_take_boxed (value, image);

  if (slice != NULL)
    gtk_border_free (slice);

  if (width != NULL)
    gtk_border_free (width);

  if (repeat != NULL)
    g_free (repeat);
}

typedef struct _GtkBorderImageSliceSize GtkBorderImageSliceSize;
struct _GtkBorderImageSliceSize {
  double offset;
  double size;
};

static void
gtk_border_image_compute_border_size (GtkBorderImageSliceSize sizes[3],
                                      double                  offset,
                                      double                  area_size,
                                      int                     start_border,
                                      int                     end_border)
{
  /* This code assumes area_size >= start_border + end_border */

  sizes[0].offset = offset;
  sizes[0].size = start_border;
  sizes[1].offset = offset + start_border;
  sizes[1].size = area_size - start_border - end_border;
  sizes[2].offset = offset + area_size - end_border;
  sizes[2].size = end_border;
}

static void
gtk_border_image_render_slice (cairo_t           *cr,
                               cairo_surface_t   *slice,
                               double             slice_width,
                               double             slice_height,
                               double             x,
                               double             y,
                               double             width,
                               double             height,
                               GtkCssBorderRepeatStyle  hrepeat,
                               GtkCssBorderRepeatStyle  vrepeat)
{
  double hscale, vscale;
  double xstep, ystep;
  cairo_extend_t extend = CAIRO_EXTEND_PAD;
  cairo_matrix_t matrix;
  cairo_pattern_t *pattern;

  /* We can't draw center tiles yet */
  g_assert (hrepeat == GTK_CSS_REPEAT_STYLE_STRETCH || vrepeat == GTK_CSS_REPEAT_STYLE_STRETCH);

  hscale = width / slice_width;
  vscale = height / slice_height;
  xstep = width;
  ystep = height;

  switch (hrepeat)
    {
    case GTK_CSS_REPEAT_STYLE_REPEAT:
      extend = CAIRO_EXTEND_REPEAT;
      hscale = vscale;
      break;
    case GTK_CSS_REPEAT_STYLE_SPACE:
      {
        double space, n;

        extend = CAIRO_EXTEND_NONE;
        hscale = vscale;

        xstep = hscale * slice_width;
        n = floor (width / xstep);
        space = (width - n * xstep) / (n + 1);
        xstep += space;
        x += space;
        width -= 2 * space;
      }
      break;
    case GTK_CSS_REPEAT_STYLE_STRETCH:
      break;
    case GTK_CSS_REPEAT_STYLE_ROUND:
      extend = CAIRO_EXTEND_REPEAT;
      hscale = width / (slice_width * MAX (round (width / (slice_width * vscale)), 1));
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  switch (vrepeat)
    {
    case GTK_CSS_REPEAT_STYLE_REPEAT:
      extend = CAIRO_EXTEND_REPEAT;
      vscale = hscale;
      break;
    case GTK_CSS_REPEAT_STYLE_SPACE:
      {
        double space, n;

        extend = CAIRO_EXTEND_NONE;
        vscale = hscale;

        ystep = vscale * slice_height;
        n = floor (height / ystep);
        space = (height - n * ystep) / (n + 1);
        ystep += space;
        y += space;
        height -= 2 * space;
      }
      break;
    case GTK_CSS_REPEAT_STYLE_STRETCH:
      break;
    case GTK_CSS_REPEAT_STYLE_ROUND:
      extend = CAIRO_EXTEND_REPEAT;
      vscale = height / (slice_height * MAX (round (height / (slice_height * hscale)), 1));
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  pattern = cairo_pattern_create_for_surface (slice);

  cairo_matrix_init_translate (&matrix,
                               hrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? slice_width / 2 : 0,
                               vrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? slice_height / 2 : 0);
  cairo_matrix_scale (&matrix, 1 / hscale, 1 / vscale);
  cairo_matrix_translate (&matrix,
                          hrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? - width / 2 : 0,
                          vrepeat == GTK_CSS_REPEAT_STYLE_REPEAT ? - height / 2 : 0);

  cairo_pattern_set_matrix (pattern, &matrix);
  cairo_pattern_set_extend (pattern, extend);

  cairo_save (cr);
  cairo_translate (cr, x, y);

  for (y = 0; y < height; y += ystep)
    {
      for (x = 0; x < width; x += xstep)
        {
          cairo_save (cr);
          cairo_translate (cr, x, y);
          cairo_set_source (cr, pattern);
          cairo_rectangle (cr, 0, 0, xstep, ystep);
          cairo_fill (cr);
          cairo_restore (cr);
        }
    }

  cairo_restore (cr);

  cairo_pattern_destroy (pattern);
}

static void
gtk_border_image_compute_slice_size (GtkBorderImageSliceSize sizes[3],
                                     int                     surface_size,
                                     int                     start_size,
                                     int                     end_size)
{
  sizes[0].size = MIN (start_size, surface_size);
  sizes[0].offset = 0;

  sizes[2].size = MIN (end_size, surface_size);
  sizes[2].offset = surface_size - sizes[2].size;

  sizes[1].size = MAX (0, surface_size - sizes[0].size - sizes[2].size);
  sizes[1].offset = sizes[0].size;
}

void
_gtk_border_image_render (GtkBorderImage   *image,
                          GtkBorder        *border_width,
                          cairo_t          *cr,
                          gdouble           x,
                          gdouble           y,
                          gdouble           width,
                          gdouble           height)
{
  cairo_surface_t *surface, *slice;
  GtkBorderImageSliceSize vertical_slice[3], horizontal_slice[3];
  GtkBorderImageSliceSize vertical_border[3], horizontal_border[3];
  double source_width, source_height;
  int h, v;

  if (image->width != NULL)
    border_width = image->width;

  _gtk_css_image_get_concrete_size (image->source,
                                    0, 0,
                                    width, height,
                                    &source_width, &source_height);

  /* XXX: Optimize for (source_width == width && source_height == height) */

  surface = _gtk_css_image_get_surface (image->source,
                                        cairo_get_target (cr),
                                        source_width, source_height);

  gtk_border_image_compute_slice_size (horizontal_slice,
                                       source_width, 
                                       image->slice.left,
                                       image->slice.right);
  gtk_border_image_compute_slice_size (vertical_slice,
                                       source_height, 
                                       image->slice.top,
                                       image->slice.bottom);
  gtk_border_image_compute_border_size (horizontal_border,
                                        x,
                                        width,
                                        border_width->left,
                                        border_width->right);
  gtk_border_image_compute_border_size (vertical_border,
                                        y,
                                        height,
                                        border_width->top,
                                        border_width->bottom);
  
  for (v = 0; v < 3; v++)
    {
      if (vertical_slice[v].size == 0 ||
          vertical_border[v].size == 0)
        continue;

      for (h = 0; h < 3; h++)
        {
          if (horizontal_slice[h].size == 0 ||
              horizontal_border[h].size == 0)
            continue;

          if (h == 1 && v == 1)
            continue;

          slice = cairo_surface_create_for_rectangle (surface,
                                                      horizontal_slice[h].offset,
                                                      vertical_slice[v].offset,
                                                      horizontal_slice[h].size,
                                                      vertical_slice[v].size);

          gtk_border_image_render_slice (cr,
                                         slice,
                                         horizontal_slice[h].size,
                                         vertical_slice[v].size,
                                         horizontal_border[h].offset,
                                         vertical_border[v].offset,
                                         horizontal_border[h].size,
                                         vertical_border[v].size,
                                         h == 1 ? image->repeat.hrepeat : GTK_CSS_REPEAT_STYLE_STRETCH,
                                         v == 1 ? image->repeat.vrepeat : GTK_CSS_REPEAT_STYLE_STRETCH);

          cairo_surface_destroy (slice);
        }
    }

  cairo_surface_destroy (surface);
}