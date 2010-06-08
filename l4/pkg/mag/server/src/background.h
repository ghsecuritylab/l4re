/*
 * (c) 2010 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/mag-gfx/canvas>
#include <l4/mag-gfx/clip_guard>

#include "view"

namespace Mag_server {

using namespace Mag_gfx;

class Background : public View
{
public:
  explicit Background(Area const &size) : View(Rect(Point(0,0), size)) {}
  void draw(Canvas *c, View_stack const *, Mode, bool) const
  {
    Clip_guard g(c, *this);
    c->draw_box(*this, Rgb32::Color(25, 37, 50));
  }

};

}