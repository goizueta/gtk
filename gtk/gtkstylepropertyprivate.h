/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_STYLEPROPERTY_PRIVATE_H__
#define __GTK_STYLEPROPERTY_PRIVATE_H__

#include "gtkcssparserprivate.h"
#include "gtkstylecontextprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_PROPERTY           (_gtk_style_property_get_type ())
#define GTK_STYLE_PROPERTY(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_STYLE_PROPERTY, GtkStyleProperty))
#define GTK_STYLE_PROPERTY_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_STYLE_PROPERTY, GtkStylePropertyClass))
#define GTK_IS_STYLE_PROPERTY(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_STYLE_PROPERTY))
#define GTK_IS_STYLE_PROPERTY_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_STYLE_PROPERTY))
#define GTK_STYLE_PROPERTY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STYLE_PROPERTY, GtkStylePropertyClass))

typedef struct _GtkStyleProperty           GtkStyleProperty;
typedef struct _GtkStylePropertyClass      GtkStylePropertyClass;

typedef enum {
  GTK_STYLE_PROPERTY_INHERIT = (1 << 0)
} GtkStylePropertyFlags;

typedef GParameter *     (* GtkStyleUnpackFunc)            (const GValue           *value,
                                                            guint                  *n_params);
typedef void             (* GtkStylePackFunc)              (GValue                 *value,
                                                            GtkStyleProperties     *props,
                                                            GtkStateFlags           state);

struct _GtkStyleProperty
{
  GObject parent;

  char *name;
  GType value_type;

  GtkStyleUnpackFunc        unpack_func;
  GtkStylePackFunc          pack_func;
};

struct _GtkStylePropertyClass
{
  GObjectClass  parent_class;
  
  void              (* assign)                             (GtkStyleProperty       *property,
                                                            GtkStyleProperties     *props,
                                                            GtkStateFlags           state,
                                                            const GValue           *value);
  void              (* query)                              (GtkStyleProperty       *property,
                                                            GtkStyleProperties     *props,
                                                            GtkStateFlags           state,
                                                            GValue                 *value);
  gboolean          (* parse_value)                        (GtkStyleProperty *      property,
                                                            GValue                 *value,
                                                            GtkCssParser           *parser,
                                                            GFile                  *base);

  GHashTable   *properties;
};

GType               _gtk_style_property_get_type             (void) G_GNUC_CONST;

GtkStyleProperty *       _gtk_style_property_lookup        (const char             *name);

const char *             _gtk_style_property_get_name      (GtkStyleProperty       *property);

gboolean                 _gtk_style_property_parse_value   (GtkStyleProperty *      property,
                                                            GValue                 *value,
                                                            GtkCssParser           *parser,
                                                            GFile                  *base);

GType                    _gtk_style_property_get_value_type(GtkStyleProperty *      property);
void                     _gtk_style_property_query         (GtkStyleProperty *      property,
                                                            GtkStyleProperties     *props,
                                                            GtkStateFlags           state,
                                                            GValue                 *value);
void                     _gtk_style_property_assign        (GtkStyleProperty       *property,
                                                            GtkStyleProperties     *props,
                                                            GtkStateFlags           state,
                                                            const GValue           *value);

G_END_DECLS

#endif /* __GTK_CSS_STYLEPROPERTY_PRIVATE_H__ */