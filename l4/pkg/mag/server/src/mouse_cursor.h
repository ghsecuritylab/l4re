/*
 * (c) 2010 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/mag-gfx/clip_guard>
#include <l4/mag-gfx/mem_texture>

#include "view_stack"
#include "view"

namespace Mag_server {

using namespace Mag_gfx;

template< typename PT >
class Mouse_cursor : public View
{
private:
  Mem::Texture<PT> const *_t;

public:
  Mouse_cursor(Texture const *t)
  : View(Rect(Point(0,0), t->size()), F_transparent),
    _t(static_cast<Mem::Texture<PT> const *>(t))
  {}

  void draw(Canvas *canvas, View_stack const *s, Mode, bool) const
  {
    Clip_guard g(canvas, *this);

    s->draw_recursive(View::next(), 0, *this);
    canvas->draw_texture(_t, Rgb32::Black, p1(), Canvas::Masked);
  }

};

}
