// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2014-2018 Gunter Königsmann <wxMaxima@physikbuch.de>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//
//  SPDX-License-Identifier: GPL-2.0+

/*! \file
  This file defines the class SubCell

  SubCell is the Cell type that represents a math element with subscript.
 */

#include "SubCell.h"
#include "VisiblyInvalidCell.h"

#define SUB_DEC 2

SubCell::SubCell(GroupCell *parent, Configuration **config) :
  Cell(parent, config),
  m_baseCell(std::make_unique<VisiblyInvalidCell>(parent,config)),
  m_indexCell(std::make_unique<VisiblyInvalidCell>(parent,config))
{
  InitBitFields();
}

SubCell::SubCell(const SubCell &cell):
    SubCell(cell.m_group, cell.m_configuration)
{
  CopyCommonData(cell);
  m_altCopyText = cell.m_altCopyText;
  if(cell.m_baseCell)
    SetBase(cell.m_baseCell->CopyList());
  if(cell.m_indexCell)
    SetIndex(cell.m_indexCell->CopyList());
}

std::unique_ptr<Cell> SubCell::Copy() const
{
  return std::make_unique<SubCell>(*this);
}

void SubCell::SetIndex(Cell *index)
{
  if (!index)
    return;
  m_indexCell.reset(index);
}

void SubCell::SetBase(Cell *base)
{
  if (!base)
    return;
  m_baseCell.reset(base);
}

void SubCell::RecalculateWidths(AFontSize fontsize)
{
  if(!NeedsRecalculation(fontsize))
    return;

  m_baseCell->RecalculateWidthsList(fontsize);
  m_indexCell->RecalculateWidthsList({ MC_MIN_SIZE, fontsize - SUB_DEC });
  m_width = m_baseCell->GetFullWidth() + m_indexCell->GetFullWidth() -
            Scale_Px(2);
  Cell::RecalculateWidths(fontsize);
}

void SubCell::RecalculateHeight(AFontSize fontsize)
{
  if(!NeedsRecalculation(fontsize))
    return;

  m_baseCell->RecalculateHeightList(fontsize);
  m_indexCell->RecalculateHeightList({ MC_MIN_SIZE, fontsize - SUB_DEC });
  m_height = m_baseCell->GetHeightList() + m_indexCell->GetHeightList() -
             Scale_Px(.8 * fontsize + MC_EXP_INDENT);
  m_center = m_baseCell->GetCenter();
  Cell::RecalculateHeight(fontsize);
}

void SubCell::Draw(wxPoint point)
{
  Cell::Draw(point);
  if (DrawThisCell(point))
  {
    wxPoint bs, in;

    bs.x = point.x;
    bs.y = point.y;
    m_baseCell->DrawList(bs);

    in.x = point.x + m_baseCell->GetFullWidth() - Scale_Px(2);
    in.y = point.y + m_baseCell->GetMaxDrop() +
           m_indexCell->GetCenterList() -
           Scale_Px(.8 * m_fontSize + MC_EXP_INDENT);
    m_indexCell->DrawList(in);
  }
}

wxString SubCell::ToString() const
{
  if (m_altCopyText != wxEmptyString)
    return m_altCopyText;

  wxString s;
  if (m_baseCell->IsCompound())
    s += wxT("(") + m_baseCell->ListToString() + wxT(")");
  else
    s += m_baseCell->ListToString();
  s += wxT("[") + m_indexCell->ListToString() + wxT("]");
  return s;
}

wxString SubCell::ToMatlab() const
{
  if (m_altCopyText != wxEmptyString)
  {
	return m_altCopyText;
  }

  wxString s;
  if (m_baseCell->IsCompound())
	s += wxT("(") + m_baseCell->ListToMatlab() + wxT(")");
  else
	s += m_baseCell->ListToMatlab();
  s += wxT("[") + m_indexCell->ListToMatlab() + wxT("]");
  return s;
}

wxString SubCell::ToTeX() const
{
  wxString s;
  wxString base = m_baseCell->ListToTeX();
  wxString index = m_indexCell->ListToTeX();
  if (base.Length() > 1)
    s = wxT("{{") + base + wxT("}_");
  else
    s = wxT("{") + base + wxT("_");
  if (index.Length() > 1)
    s += wxT("{") + index + wxT("}}");
  else
    s += index + wxT("}");
  return s;
}

wxString SubCell::ToMathML() const
{
  return wxT("<msub>") +
         m_baseCell->ListToMathML() +
         m_indexCell->ListToMathML() +
         wxT("</msub>\n");
}

wxString SubCell::ToOMML() const
{
  return wxT("<m:sSub><m:e>") + m_baseCell->ListToOMML() + wxT("</m:e><m:sub>") +
         m_indexCell->ListToOMML() + wxT("</m:sub></m:sSub>\n");
}

wxString SubCell::ToXML() const
{
  wxString flags;
  if (m_forceBreakLine)
    flags += wxT(" breakline=\"true\"");

  if (m_altCopyText != wxEmptyString)
    flags += wxT(" altCopy=\"") + XMLescape(m_altCopyText) + wxT("\"");
  
  return wxT("<i") + flags + wxT("><r>") + m_baseCell->ListToXML() + wxT("</r><r>") +
           m_indexCell->ListToXML() + wxT("</r></i>");
}
