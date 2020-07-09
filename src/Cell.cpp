// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//  Copyright (C) 2014-2018 Gunter Königsmann <wxMaxima@physikbuch.de>
//  Copyright (C) 2020      Kuba Ober <kuba@bertec.com>
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
  This file defines the class Cell

  Cell is the base class for all cell- or list-type elements.
*/

#include "Cell.h"
#include "GroupCell.h"
#include "TextCell.h"
#include <wx/regex.h>
#include <wx/sstream.h>
#include <wx/xml/xml.h>

void Cell::CellPointers::ErrorList::Remove(GroupCell * cell)
{
  m_errors.erase(std::remove(m_errors.begin(), m_errors.end(), cell), m_errors.end());
}

bool Cell::CellPointers::ErrorList::Contains(GroupCell * cell) const
{
  return std::find(m_errors.begin(), m_errors.end(), cell) != m_errors.end();
}

void Cell::CellPointers::ErrorList::Add(GroupCell * cell)
{ m_errors.emplace_back(cell); }

void Cell::CellPointers::SetWorkingGroup(GroupCell *group)
{
  if (group)
    m_lastWorkingGroup = group;
  m_workingGroup = group;
}

GroupCell *Cell::CellPointers::GetWorkingGroup(bool resortToLast) const
{ return (m_workingGroup || !resortToLast) ? m_workingGroup : m_lastWorkingGroup; }

GroupCell *Cell::CellPointers::ErrorList::FirstError() const
{ return m_errors.empty() ? nullptr : m_errors.front().get(); }

GroupCell *Cell::CellPointers::ErrorList::LastError() const
{ return m_errors.empty() ? nullptr : m_errors.back().get(); }

wxString Cell::GetToolTip(const wxPoint point)
{
  if (!ContainsPoint(point))
    return {};

  wxString toolTip;
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
    for (Cell *tmp = cell; tmp; tmp = tmp->m_next)
      if (!(toolTip = tmp->GetToolTip(point)).IsEmpty())
        return toolTip;

  return m_toolTip;
}

Cell::Cell(GroupCell *group, Configuration **config) :
    m_toolTip((*config)->GetDefaultCellToolTip()),
    m_group(group),
    m_configuration(config),
    m_cellPointers(GetCellPointers()),
    m_fontSize((*config)->GetMathFontSize())
{
}

Cell::~Cell()
{
  // Delete this list of cells without using a recursive function call that can
  // run us out of stack space
  Cell *next = m_next;
  while (next != NULL)
  {
    Cell *cell = next;
    next = next->m_next;
    cell->m_next = NULL;
    wxDELETE(cell);
  }
}

void Cell::SetType(CellType type)
{
  m_type = type;

  switch (m_type)
  {
    case MC_TYPE_MAIN_PROMPT:
      m_textStyle = TS_MAIN_PROMPT;
      break;
    case MC_TYPE_PROMPT:
      m_textStyle = TS_OTHER_PROMPT;
      break;
    case MC_TYPE_LABEL:
      m_textStyle = TS_LABEL;
      HardLineBreak();
      break;
    case MC_TYPE_INPUT:
      m_textStyle = TS_INPUT;
      break;
    case MC_TYPE_ERROR:
      m_textStyle = TS_ERROR;
      break;
    case MC_TYPE_WARNING:
      m_textStyle = TS_WARNING;
      break;
    case MC_TYPE_TEXT:
      m_textStyle = TS_TEXT;
      break;
    case MC_TYPE_SUBSUBSECTION:
      m_textStyle = TS_SUBSUBSECTION;
      break;
    case MC_TYPE_HEADING5:
      m_textStyle = TS_HEADING5;
      break;
    case MC_TYPE_HEADING6:
      m_textStyle = TS_HEADING6;
      break;
    case MC_TYPE_SUBSECTION:
      m_textStyle = TS_SUBSECTION;
      break;
    case MC_TYPE_SECTION:
      m_textStyle = TS_SECTION;
      break;
    case MC_TYPE_TITLE:
      m_textStyle = TS_TITLE;
      break;
    default:
      m_textStyle = TS_DEFAULT;
      break;
  }
  ResetSize();
  if (m_group)
    GetGroup()->ResetSize();
}

void Cell::CopyCommonData(const Cell & cell)
{
  m_toolTip = cell.m_toolTip;
  m_forceBreakLine = cell.m_forceBreakLine;
  m_type = cell.m_type;
  m_textStyle = cell.m_textStyle;
  m_isHidden = cell.m_isHidden;
  m_isHidableMultSign = cell.m_isHidableMultSign;
}

Cell *Cell::CopyList() const
{
  Cell *dest = Copy();
  Cell *ret = dest;
  Cell *src = m_next;

  while (src != NULL)
  {
    dest->AppendCell(src->Copy());
    src = src->m_next;
    dest = dest->m_next;
  }
  return ret;
}

void Cell::ClearCacheList()
{
  for(Cell *tmp = this; tmp != NULL; tmp = tmp->m_next)
    tmp->ClearCache();
}

void Cell::SetGroupList(GroupCell *group)
{
  for (Cell *tmp = this; tmp != NULL; tmp = tmp->m_next)
    tmp->SetGroup(group);
}

int Cell::CellsInListRecursive() const
{
  //! The number of cells the current group contains (-1, if no GroupCell)
  int cells = 0;

  for (const Cell *tmp = this; tmp; tmp = tmp->m_next)
  {
    ++ cells;
    for (auto cell = tmp->InnerBegin(); cell != tmp->InnerEnd(); ++ cell)
    {
      if (cell)
        // I believe with the if(cell) we cannot use std::accumulate here.
        // cppcheck-suppress useStlAlgorithm
        cells += cell->CellsInListRecursive();
    }
  }
  return cells;
}

void Cell::SetGroup(GroupCell *group)
{
  m_group = group;
  if (group)
    wxASSERT (group->GetType() == MC_TYPE_GROUP);
  
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
    if (cell)
      cell->SetGroupList(group);
}

void Cell::FontsChangedList()
{
  for (Cell *tmp = this; tmp; tmp = tmp->m_next)
  {
    tmp->FontsChanged();
    for (auto cell = tmp->InnerBegin(); cell != tmp->InnerEnd(); ++ cell)
      if (cell)
        cell->FontsChangedList();
  }
}


/***
 * Append new cell to the end of this list.
 */
void Cell::AppendCell(Cell *p_next)
{
  if (p_next == NULL)
    return;
  if(m_group)
    GetGroup()->ResetData();

  Cell *LastInList = last();

  // Append this p_next to the list
  LastInList->m_next = p_next;
  LastInList->m_next->m_previous = LastInList;
  
  // Search the last cell in the list that is sorted by the drawing order
  Cell *LastToDraw = LastInList;
  while (LastToDraw->GetNextToDraw() != NULL)
    LastToDraw = LastToDraw->GetNextToDraw();

  // Append p_next to this list.
  LastToDraw->SetNextToDraw(p_next);
}

GroupCell *Cell::GetGroup() const
{
  GroupCell *group = m_group;
  wxASSERT_MSG(group, _("Bug: Math Cell that claims to have no group Cell it belongs to"));
  return group;
}

/***
 * Get the maximum drop of the center.
 */
int Cell::GetCenterList()
{
  if (m_recalculate_maxCenter || ((*m_configuration)->RecalculationForce()))
  {
    m_recalculate_maxCenter = false;
    Cell *tmp = this;
    m_maxCenter  = 0;
    while (tmp != NULL)
    {
      if ((tmp != this) && (tmp->m_breakLine))
        break;
      if(!tmp->m_isBrokenIntoLines)
        m_maxCenter = wxMax(m_maxCenter, tmp->m_center);
      tmp = tmp->GetNextToDraw();
    }
  }
  return m_maxCenter;
}

bool Cell::NeedsRecalculation(AFontSize fontSize) const
{
  bool result = (m_recalculateWidths) ||
    (fontSize != m_fontsize_old) ||
    (m_isBrokenIntoLines != m_isBrokenIntoLines_old) ||
    (m_clientWidth_old != (*m_configuration)->GetClientWidth()) ||
    (m_lastZoomFactor != (*m_configuration)->GetZoomFactor()) ||
    ((*m_configuration)->RecalculationForce()) ||
    (*m_configuration)->FontChanged();
  return result;
}

/***
 * Get the maximum drop of cell.
 */
int Cell::GetMaxDrop()
{
  if (m_recalculate_maxDrop || ((*m_configuration)->RecalculationForce()))
  {
    m_recalculate_maxDrop = false;
    m_maxDrop = 0;
    Cell *tmp = this;
    while (tmp != NULL)
    {
      if ((tmp != this) && (tmp->m_breakLine))
        break;
      if(!tmp->m_isBrokenIntoLines)
        m_maxDrop = wxMax(m_maxDrop, tmp->m_height - tmp->m_center);
      tmp = tmp->GetNextToDraw();
    }
  }
  return m_maxDrop;
}

//!  Get the maximum hight of cells in line.
int Cell::GetHeightList()
{
  return GetCenterList() + GetMaxDrop();
}

/*! Get full width of this group.
 */
int Cell::GetFullWidth()
{
  // Recalculate the with of this list of cells only if this has been marked as necessary.
  if (m_recalculate_maxWidth || ((*m_configuration)->RecalculationForce()))
  {
    m_recalculate_maxWidth = false;
    Cell *tmp = this;

    // We begin this calculation with a negative offset since the full width of only a single
    // cell doesn't contain the space that separates two cells - that is automatically added
    // to every cell in the next step.
    m_fullWidth = 0;
    while (tmp != NULL)
    {
      m_fullWidth += tmp->m_width;
      tmp = tmp->GetNextToDraw();
    }
  }
  return m_fullWidth;
}

/*! Get the width of this line.
 */
int Cell::GetLineWidth()
{
  if (m_recalculate_lineWidth)
  {
    m_recalculate_lineWidth = false;
    m_lineWidth = 0;
    int width = m_width;

    Cell *tmp = this;
    while(tmp != NULL)
    {
      width += tmp->m_width;

      if (width > m_lineWidth)
        m_lineWidth = width;

      tmp = tmp->GetNextToDraw();
      if(tmp != NULL)
      {
        if(tmp->m_isBrokenIntoLines || tmp->m_breakLine || (tmp->m_type == MC_TYPE_MAIN_PROMPT))
          break;
      }
    }
  }
  return m_lineWidth;
}

/*! Draw this cell to dc

 To make this work each derived class must draw the content of the cell
 and then call MathCall::Draw(...).
 */
void Cell::Draw(wxPoint point)
{
  Configuration *configuration = *m_configuration;
  configuration->NotifyOfCellRedraw(this);

  if((point.x >= 0) && (point.y >= 0))
    SetCurrentPoint(point);
  
  // Mark all cells that contain tooltips
  if (!m_toolTip.empty() && (GetStyle() != TS_LABEL) && (GetStyle() != TS_USERLABEL) &&
      configuration->ClipToDrawRegion() && !configuration->GetPrinting() &&(!m_group->m_suppressTooltipMarker))
  {
    wxRect rect = Cell::CropToUpdateRegion(GetRect());
    if (Cell::InUpdateRegion(rect))
    {
      if((rect.GetWidth() > 0) && rect.GetHeight() > 0)
      {
        wxDC *dc = configuration->GetDC();
        dc->SetPen(*wxTRANSPARENT_PEN);
        dc->SetBrush((*m_configuration)->GetTooltipBrush());
        dc->DrawRectangle(rect);
      }
    }
  }
  
  // Tell the screen reader that this cell's contents might have changed.
#if wxUSE_ACCESSIBILITY
  if (configuration->GetWorkSheet())
    NotifyEvent(0, configuration->GetWorkSheet(), wxOBJID_CLIENT, wxOBJID_CLIENT);
#endif
}

void Cell::SetToolTip(const wxString &tooltip)
{
  m_toolTip = tooltip;
  m_containsToolTip = (!tooltip.IsEmpty());
  if (m_group)
    m_group->m_containsToolTip = m_containsToolTip;
}

void Cell::AddToolTip(const wxString &tip)
{
  if(tip.IsEmpty())
    return;
  if((!m_toolTip.IsEmpty()) && (!m_toolTip.EndsWith("\n")))
    m_toolTip += "\n";
  m_toolTip += tip;
  m_containsToolTip = true;
  m_group->m_containsToolTip = true;
}
void Cell::DrawList(wxPoint point)
{
  Cell *tmp = this;
  while (tmp != NULL)
  {
    tmp->Draw(point);
    point.x += tmp->m_width;
    wxASSERT(tmp != tmp->GetNextToDraw());
    tmp = tmp->GetNextToDraw();
  }
}

void Cell::RecalculateList(AFontSize fontsize)
{
  Cell *tmp = this;

  while (tmp != NULL)
  {
    tmp->RecalculateWidths(fontsize);
    tmp->RecalculateHeight(fontsize);
    tmp = tmp->m_next;
  }
}

void Cell::ResetSizeList()
{
  for(Cell *tmp = this; tmp != NULL; tmp = tmp->m_next)
    tmp->ResetSize();
}


void Cell::RecalculateHeightList(AFontSize fontsize)
{
  Cell *tmp = this;

  while (tmp != NULL)
  {
    tmp->RecalculateHeight(fontsize);
    tmp = tmp->m_next;
  }
}

/*! Recalculate widths of cells.

  (Used for changing font size since in this case all size information has to be
  recalculated).

  Should set: set m_width.
*/
void Cell::RecalculateWidthsList(AFontSize fontsize)
{
  Cell *tmp = this;

  while (tmp != NULL)
  {
    tmp->RecalculateWidths(fontsize);
    tmp = tmp->m_next;
  }
}

void Cell::RecalculateWidths(AFontSize WXUNUSED(fontsize))
{
}

void Cell::RecalculateHeight(AFontSize fontsize)
{
  m_fontSize = fontsize;
  m_fontsize_old = fontsize;
  m_isBrokenIntoLines_old = m_isBrokenIntoLines;
  m_clientWidth_old = (*m_configuration)->GetClientWidth();
  m_lastZoomFactor = (*m_configuration)->GetZoomFactor();
  ResetCellListSizes();
  m_recalculateWidths = false;
}

/*! Is this cell currently visible in the window?.
 */
bool Cell::DrawThisCell(wxPoint point)
{
  // If the cell isn't on the worksheet we don't draw it.
  if((point.x < 0) || (point.y < 0))
    return false;

  SetCurrentPoint(point);

  // If a cell is broken into lines the cells it contains are displayed but
  // not the cell itself (example: Denominator and Numerator are displayed
  // but not the horizontal line with denominator above and numerator below.
  if(m_isBrokenIntoLines)
    return false;
  
  if(!(*m_configuration)->ClipToDrawRegion())
    return true;
  
  return(InUpdateRegion());
}

wxRect Cell::GetRect(bool wholeList)
{
  if (wholeList)
    return wxRect(m_currentPoint.x, m_currentPoint.y - GetCenterList(),
                  GetLineWidth(), GetHeightList());
  else
    return wxRect(m_currentPoint.x, m_currentPoint.y - m_center,
                  m_width, m_height);
}

bool Cell::InUpdateRegion(const wxRect &rect)
{
  if (!(*m_configuration)->ClipToDrawRegion())
    return true;

  if((m_currentPoint.x < 0) || (m_currentPoint.y < 0))
    return false;
  
  wxRect updateRegion = (*m_configuration)->GetUpdateRegion();

  // If we have deferred the recalculation of the cell height but now
  // got a draw request due to moving the mouse wheel we need to guess
  // the cell size 
  if(m_height < 0)
  {
    int height = 0;
    if(m_next)
      height = m_next->m_currentPoint.y - m_currentPoint.y;

    if ((updateRegion.GetBottom() >= m_currentPoint.y) &&
        (updateRegion.GetTop() <= m_currentPoint.y+height))
    return true;
  }

  if(updateRegion.Contains(m_currentPoint))
    return true;

  return updateRegion.Intersects(rect) ||
    updateRegion.Contains(rect) ||
    (updateRegion == rect) || rect.Contains(updateRegion);
}

void Cell::DrawBoundingBox(wxDC &dc, bool all)
{
  wxRect rect = GetRect(all);
  if (InUpdateRegion())
  {
    dc.DrawRectangle(CropToUpdateRegion(rect));
  }
}

bool Cell::IsCompound() const
{
  for (const Cell *tmp = this; tmp; tmp = tmp->GetNext())
    if (tmp->IsOperator())
      return true;
  return false;
}

/***
 * Return the string representation of cell.
 */
wxString Cell::ToString()
{
  return wxEmptyString;
}

wxString Cell::VariablesAndFunctionsList()
{
  wxString retval;
  Cell *tmp = this;
  while (tmp != NULL)
  {
    if(
      (tmp->GetStyle() == TS_LABEL) ||
      (tmp->GetStyle() == TS_USERLABEL) ||
      (tmp->GetStyle() == TS_MAIN_PROMPT) ||
      (tmp->GetStyle() == TS_VARIABLE) ||
      (tmp->GetStyle() == TS_FUNCTION))
      retval += tmp->ToString() + " ";      
    tmp = tmp->GetNextToDraw();
  }
  return retval;
}

wxString Cell::ListToString()
{
  wxString retval;
  Cell *tmp = this;
  bool firstline = true;

  while (tmp != NULL)
  {
    if ((!firstline) && (tmp->m_forceBreakLine))
    {
      if(!retval.EndsWith(wxT('\n')))
        retval += wxT("\n");
      // if(
      //    (tmp->GetStyle() != TS_LABEL) &&
      //    (tmp->GetStyle() != TS_USERLABEL) &&
      //    (tmp->GetStyle() != TS_MAIN_PROMPT) &&
      //    (tmp->GetStyle() != TS_OTHER_PROMPT))
      //   retval += wxT("\t");
    }
    // if(firstline)
    // {
    //   if((tmp->GetStyle() != TS_LABEL) &&
    //      (tmp->GetStyle() != TS_USERLABEL) &&
    //      (tmp->GetStyle() != TS_MAIN_PROMPT) &&
    //      (tmp->GetStyle() != TS_OTHER_PROMPT))
    //     retval += wxT("\t");
    // }
    retval += tmp->ToString();

    firstline = false;
    tmp = tmp->GetNextToDraw();
  }
  return retval;
}

wxString Cell::ToMatlab()
{
  return wxEmptyString;
}

wxString Cell::ListToMatlab()
{
	wxString retval;
	Cell *tmp = this;
	bool firstline = true;

	while (tmp != NULL)
	{
	  if ((!firstline) && (tmp->m_forceBreakLine))
	  {
		if(!retval.EndsWith(wxT('\n')))
		  retval += wxT("\n");
		// if(
		//    (tmp->GetStyle() != TS_LABEL) &&
		//    (tmp->GetStyle() != TS_USERLABEL) &&
		//    (tmp->GetStyle() != TS_MAIN_PROMPT) &&
		//    (tmp->GetStyle() != TS_OTHER_PROMPT))
		//   retval += wxT("\t");
	  }
	  // if(firstline)
	  // {
	  //   if((tmp->GetStyle() != TS_LABEL) &&
	  //      (tmp->GetStyle() != TS_USERLABEL) &&
	  //      (tmp->GetStyle() != TS_MAIN_PROMPT) &&
	  //      (tmp->GetStyle() != TS_OTHER_PROMPT))
	  //     retval += wxT("\t");
	  // }
	  retval += tmp->ToMatlab();

	  firstline = false;
	  tmp = tmp->GetNextToDraw();
	}

	return retval;
}

wxString Cell::ToTeX()
{
  return wxEmptyString;
}

wxString Cell::ListToTeX()
{
  wxString retval;
  Cell *tmp = this;

  while (tmp != NULL)
  {
    if ((tmp->m_textStyle == TS_LABEL && retval != wxEmptyString) ||
        (tmp->m_breakLine && retval != wxEmptyString))
      retval += wxT("\\]\\[");
    retval += tmp->ToTeX();
    tmp = tmp->m_next;
  }
  return retval;
}

wxString Cell::ToXML()
{
  return wxEmptyString;
}

wxString Cell::ToMathML()
{
  return wxEmptyString;
}

wxString Cell::ListToMathML(bool startofline)
{
  bool highlight = false;

  wxString retval;

  // If the region to export contains linebreaks or labels we put it into a table.
  bool needsTable = false;
  Cell *temp = this;
  while (temp)
  {
    if (temp->HardLineBreak())
      needsTable = true;

    if (temp->GetType() == MC_TYPE_LABEL)
      needsTable = true;

    temp = temp->m_next;
  }

  temp = this;
  // If the list contains multiple cells we wrap them in a <mrow> in order to
  // group them into a single object.
  bool multiCell = (temp->m_next != NULL);

  // Export all cells
  while (temp != NULL)
  {
    // Do we need to end a highlighting region?
    if ((!temp->m_highlight) && (highlight))
      retval += wxT("</mrow>");

    // Handle linebreaks
    if ((temp != this) && (temp->HardLineBreak()))
      retval += wxT("</mtd></mlabeledtr>\n<mlabeledtr columnalign=\"left\"><mtd>");

    // If a linebreak isn't followed by a label we need to introduce an empty one.
    if ((((temp->HardLineBreak()) || (startofline && (this == temp))) &&
         ((temp->GetStyle() != TS_LABEL) && (temp->GetStyle() != TS_USERLABEL))) && (needsTable))
      retval += wxT("<mtext></mtext></mtd><mtd>");

    // Do we need to start a highlighting region?
    if ((temp->m_highlight) && (!highlight))
      retval += wxT("<mrow mathcolor=\"red\">");
    highlight = temp->m_highlight;


    retval += temp->ToMathML();
    temp = temp->m_next;
  }

  // If the region we converted to MathML ended within a highlighted region
  // we need to close this region now.
  if (highlight)
    retval += wxT("</mrow>");

  // If we grouped multiple cells as a single object we need to cose this group now
  if ((multiCell) && (!needsTable))
    retval = wxT("<mrow>") + retval + wxT("</mrow>\n");

  // If we put the region we exported into a table we need to end this table now
  if (needsTable)
    retval = wxT("<mtable>\n<mlabeledtr columnalign=\"left\"><mtd>") + retval + wxT("</mtd></mlabeledtr>\n</mtable>");
  return retval;
}

wxString Cell::OMML2RTF(wxXmlNode *node)
{
  wxString result;

  while (node != NULL)
  {
    if (node->GetType() == wxXML_ELEMENT_NODE)
    {
      wxString ommlname = node->GetName();
      result += wxT("{\\m") + ommlname.Right(ommlname.Length() - 2);

      // Convert the attributes
      wxXmlAttribute *attributes = node->GetAttributes();
      while (attributes != NULL)
      {
        wxString ommlatt = attributes->GetName();
        result += wxT("{\\m") + ommlatt.Right(ommlatt.Length() - 2) +
                  wxT(" ") + attributes->GetValue() + wxT("}");
        attributes = attributes->GetNext();
      }

      // Convert all child nodes
      if (node->GetChildren() != NULL)
      {
        result += OMML2RTF(node->GetChildren());
      }
      result += wxT("}");
    }
    else
      result += wxT(" ") + RTFescape(node->GetContent());

    node = node->GetNext();
  }
  return result;
}

wxString Cell::OMML2RTF(wxString ommltext)
{
  if (ommltext == wxEmptyString)
    return wxEmptyString;

  wxString result;
  wxXmlDocument ommldoc;
  ommltext = wxT("<m:r>") + ommltext + wxT("</m:r>");

  wxStringInputStream ommlStream(ommltext);

  ommldoc.Load(ommlStream, wxT("UTF-8"));

  wxXmlNode *node = ommldoc.GetRoot();
  result += OMML2RTF(node);

  if ((result != wxEmptyString) && (result != wxT("\\mr")))
  {
    result = wxT("{\\mmath {\\*\\moMath") + result + wxT("}}");
  }
  return result;
}

wxString Cell::XMLescape(wxString input)
{
  input.Replace(wxT("&"), wxT("&amp;"));
  input.Replace(wxT("<"), wxT("&lt;"));
  input.Replace(wxT(">"), wxT("&gt;"));
  input.Replace(wxT("'"), wxT("&apos;"));
  input.Replace(wxT("\""), wxT("&quot;"));
  return input;
}

wxString Cell::RTFescape(wxString input, bool MarkDown)
{
  // Characters with a special meaning in RTF
  input.Replace("\\", "\\\\");
  input.Replace("{", "\\{");
  input.Replace("}", "\\}");
  input.Replace(wxT("\r"), "\n");

  // The Character we will use as a soft line break
  input.Replace("\r", wxEmptyString);

  // Encode unicode characters in a rather mind-boggling way
  wxString output;
  for (size_t i = 0; i < input.Length(); i++)
  {
    wxChar ch = input[i];
    if (ch == wxT('\n'))
    {
      if (((i > 0) && (input[i - 1] == wxT('\n'))) || !MarkDown)
        output += wxT("\\par}\n{\\pard ");
      else
        output += wxT("\n");
    }
    else
    {
      if ((ch < 128) && (ch > 0))
      {
        output += ch;
      }
      else
      {
        if (ch < 32768)
        {
          output += wxString::Format("\\u%i?", int(ch));
        }
        else
        {
          output += wxString::Format("\\u%i?", int(ch) - 65536);
        }
      }
    }
  }
  return (output);
}

wxString Cell::ListToOMML(bool WXUNUSED(startofline))
{
  bool multiCell = (m_next != NULL);

  wxString retval;

  // If the region to export contains linebreaks or labels we put it into a table.
  // Export all cells

  Cell *tmp = this;
  while (tmp != NULL)
  {
    wxString token = tmp->ToOMML();

    // End exporting the equation if we reached the end of the equation.
    if (token == wxEmptyString)
      break;

    retval += token;

    // Hard linebreaks aren't supported by OMML and therefore need a new equation object
    if (tmp->HardLineBreak())
      break;

    tmp = tmp->m_next;
  }

  if ((multiCell) && (retval != wxEmptyString))
    return wxT("<m:r>") + retval + wxT("</m:r>");
  else
    return retval;
}

wxString Cell::ListToRTF(bool startofline)
{
  wxString retval;
  Cell *tmp = this;

  while (tmp != NULL)
  {
    wxString rtf = tmp->ToRTF();
    if (rtf != wxEmptyString)
    {
      if ((GetStyle() == TS_LABEL) || ((GetStyle() == TS_USERLABEL)))
      {
        retval += wxT("\\par}\n{\\pard\\s22\\li1105\\lin1105\\fi-1105\\f0\\fs24 ") + rtf + wxT("\\tab");
        startofline = false;
      }
      else
      {
        if (startofline)
          retval += wxT("\\par}\n{\\pard\\s21\\li1105\\lin1105\\f0\\fs24 ") + rtf + wxT("\\n");
        startofline = true;
      }
      tmp = tmp->m_next;
    }
    else
    {
      if (tmp->ListToOMML() != wxEmptyString)
      {
        // Math!

        // set the style for this line.
        if (startofline)
          retval += wxT("\\pard\\s21\\li1105\\lin1105\\f0\\fs24 ");

        retval += OMML2RTF(tmp->ListToOMML());

        startofline = true;

        // Skip the rest of this equation
        while (tmp != NULL)
        {
          // A non-equation item starts a new rtf item
          if (tmp->ToOMML() == wxEmptyString)
            break;

          // A newline starts a new equation
          if (tmp->HardLineBreak())
          {
            tmp = tmp->m_next;
            break;
          }

          tmp = tmp->m_next;
        }
      }
      else
      {
        tmp = tmp->m_next;
      }
    }
  }
  return retval;
}

void Cell::SelectPointText(wxPoint WXUNUSED(point)) {}

void Cell::SelectRectText(wxPoint WXUNUSED(one), wxPoint WXUNUSED(two)) {}

void Cell::PasteFromClipboard(bool WXUNUSED(primary)) {}

wxString Cell::ListToXML()
{
  bool highlight = false;

  wxString retval;
  Cell *tmp = this;

  while (tmp != NULL)
  {
    if ((tmp->GetHighlight()) && (!highlight))
    {
      retval += wxT("<hl>\n");
      highlight = true;
    }

    if ((!tmp->GetHighlight()) && (highlight))
    {
      retval += wxT("</hl>\n");
      highlight = false;
    }

    retval += tmp->ToXML();
    tmp = tmp->m_next;
  }

  if (highlight)
  {
    retval += wxT("</hl>\n");
  }

  return retval;
}

/***
 * Get the part for diff tag support - only ExpTag overvrides this.
 */
wxString Cell::GetDiffPart() const
{
  return wxEmptyString;
}

/***
 * Find the first and last cell in rectangle rect in this line.
 */
void Cell::SelectRect(const wxRect &rect, CellPtr<Cell> *first, CellPtr<Cell> *last)
{
  SelectFirst(rect, first);
  if (*first)
  {
    *last = *first;
    (*first)->SelectLast(rect, last);
    if (*last == *first)
      (*first)->SelectInner(rect, first, last);
  }
  else
    *last = nullptr;
}

/***
 * Find the first cell in rectangle rect in this line.
 */
void Cell::SelectFirst(const wxRect &rect, CellPtr<Cell> *first)
{
  if (rect.Intersects(GetRect(false)))
    *first = this;
  else if (GetNextToDraw())
    GetNextToDraw()->SelectFirst(rect, first);
  else
    *first = nullptr;
}

/***
 * Find the last cell in rectangle rect in this line.
 */
void Cell::SelectLast(const wxRect &rect, CellPtr<Cell> *last)
{
  if (rect.Intersects(GetRect(false)))
    *last = this;
  if (GetNextToDraw())
    GetNextToDraw()->SelectLast(rect, last);
}

/***
 * Select rectangle in deeper cell
 */
void Cell::SelectInner(const wxRect &rect, CellPtr<Cell> *first, CellPtr<Cell> *last)
{
  *first = nullptr;
  *last = nullptr;

  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
    for (Cell *tmp = cell; tmp; tmp = tmp->m_next)
      if (tmp->ContainsRect(rect))
        tmp->SelectRect(rect, first, last);

  if (!*first || !*last)
  {
    *first = this;
    *last = this;
  }
}

bool Cell::ContainsRect(const wxRect &sm, bool all)
{
  wxRect big = GetRect(all);
  if (big.x <= sm.x &&
      big.y <= sm.y &&
      big.x + big.width >= sm.x + sm.width &&
      big.y + big.height >= sm.y + sm.height)
    return true;
  return false;
}

/*! Resets remembered size and position info for this cell and all cells inside it

 Resets cached data like width and the height of the current cell
 as well as the vertical position of the center. Then repeats this
 with
 */
void Cell::ResetData()
{
  ResetSize();
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
    for (Cell *tmp = cell; tmp; tmp = tmp->m_next)
      tmp->ResetData();
}

Cell *Cell::first() const
{
  const Cell *tmp = this;
  while (tmp->m_previous)
    tmp = tmp->m_previous;

  wxASSERT(tmp);
  return const_cast<Cell*>(tmp);
}

Cell *Cell::last() const
{
  const Cell *tmp = this;
  while (tmp->m_next)
    tmp = tmp->m_next;

  wxASSERT(tmp);
  return const_cast<Cell*>(tmp);
}

bool Cell::BreakUp()
{
  bool retval = false;
  int clientWidth = .8*(*m_configuration)->GetClientWidth() - (*m_configuration)->GetIndent();
  if(clientWidth < 50)
    clientWidth = 50;
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
    for (Cell *tmp = cell; tmp; tmp = tmp->m_next)
      if(tmp->GetWidth() > clientWidth)
      {
        tmp->BreakUp();
        retval = true;
    }
  return retval;
}

void Cell::Unbreak()
{
  if(m_isBrokenIntoLines)
    ResetData();

  m_isBrokenIntoLines = false;
  SetNextToDraw(m_next);

  // Unbreak the inner cells, too
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
    for (Cell *tmp = cell; tmp; tmp = tmp->m_next)
      tmp->Unbreak();
}

void Cell::UnbreakList()
{
  for(Cell *tmp = this; tmp != NULL; tmp = tmp->m_next)
    tmp->Unbreak();
}

// cppcheck-suppress functionStatic
// cppcheck-suppress functionConst
// Set the pen in device context according to the style of the cell.
void Cell::SetPen(double lineWidth)
{
  Configuration *configuration = (*m_configuration);
  wxDC *dc = configuration->GetDC();

  wxPen pen;

  if (m_highlight)
    pen = *(wxThePenList->FindOrCreatePen(configuration->GetColor(TS_HIGHLIGHT),
                                          lineWidth * configuration->GetDefaultLineWidth(), wxPENSTYLE_SOLID));
  else if (m_type == MC_TYPE_PROMPT)
    pen = *(wxThePenList->FindOrCreatePen(configuration->GetColor(TS_OTHER_PROMPT),
                                              lineWidth * configuration->GetDefaultLineWidth(), wxPENSTYLE_SOLID));
  else if (m_type == MC_TYPE_INPUT)
    pen = *(wxThePenList->FindOrCreatePen(configuration->GetColor(TS_INPUT),
                                          lineWidth * configuration->GetDefaultLineWidth(), wxPENSTYLE_SOLID));
  else
    pen = *(wxThePenList->FindOrCreatePen(configuration->GetColor(TS_DEFAULT),
                                          lineWidth * configuration->GetDefaultLineWidth(), wxPENSTYLE_SOLID));

  dc->SetPen(pen);
  if(configuration->GetAntialiassingDC() != dc)
    configuration->GetAntialiassingDC()->SetPen(pen);
}

const wxString &Cell::GetValue() const
{
  static const wxString empty;
  return empty;
}

void Cell::SetForeground()
{
  Configuration *configuration = (*m_configuration);
  wxColour color;
  wxDC *dc = configuration->GetDC();
  if (m_highlight)
  {
    color = configuration->GetColor(TS_HIGHLIGHT);
  }
  else
  {
    switch (m_type)
    {
      case MC_TYPE_PROMPT:
        color = configuration->GetColor(TS_OTHER_PROMPT);
        break;
      case MC_TYPE_MAIN_PROMPT:
        color = configuration->GetColor(TS_MAIN_PROMPT);
        break;
      case MC_TYPE_ERROR:
        color = wxColour(wxT("red"));
        break;
      case MC_TYPE_WARNING:
        color = configuration->GetColor(TS_WARNING);
        break;
      case MC_TYPE_LABEL:
        color = configuration->GetColor(TS_LABEL);
        break;
      default:
        color = configuration->GetColor(m_textStyle);
        break;
    }
  }

  dc->SetTextForeground(color);
}

bool Cell::IsMath() const
{
  return !(m_textStyle == TS_LABEL ||
           m_textStyle == TS_USERLABEL ||
           m_textStyle == TS_INPUT);
}

#if wxUSE_ACCESSIBILITY
wxAccStatus Cell::GetDescription(int childId, wxString *description)
{
  if (!description)
    return wxACC_FAIL;

  if (childId == 0)
    return (*description = _("Math output")), wxACC_OK;

  wxAccessible *cell = {};
  if (GetChild(childId, &cell) == wxACC_OK && cell)
    return cell->GetDescription(0, description);

  return (description->clear()), wxACC_FAIL;
}

wxAccStatus Cell::GetParent (wxAccessible **parent)
{
  if (!parent)
    return wxACC_FAIL;

  if (*parent != this)
    return (*parent = m_group), wxACC_OK;

  if ((*m_configuration)->GetWorkSheet())
    *parent = (*m_configuration)->GetWorkSheet()->GetAccessible();

  return wxACC_OK;
}

wxAccStatus Cell::GetValue (int childId, wxString *strValue)
{
  if (!strValue)
    return wxACC_FAIL;

  wxAccessible *cell;
  if (GetChild(childId, &cell) == wxACC_OK)
    return (*strValue = static_cast<Cell*>(cell)->ToString()), wxACC_OK;

  return (strValue->clear()), wxACC_FAIL;
}

wxAccStatus Cell::GetChildCount(int *childCount)
{
  if (!childCount)
    return wxACC_FAIL;

  int count = 0;
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
    count += (cell ? 1 : 0);

  return (*childCount = count), wxACC_OK;
}

wxAccStatus Cell::HitTest(const wxPoint &pt, int *childId, wxAccessible **child)
{
  wxRect rect;
  GetLocation(rect, 0);
  // If this cell doesn't contain the point none of the sub-cells does.
  if (!rect.Contains(pt))
    return (childId && (*childId = 0)), (child && (*child = NULL)), //-V560
           wxACC_FAIL;

  int id = 0; // Child #0 is this very cell
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
  {
    // GetChildCount(), GetChild(), and this loop all skip null children - thus
    // the child identifiers present the same via the accessibility API.
    if (!cell)
      continue;
    ++ id; // The first valid inner cell will have id #1, and so on.

    cell->GetLocation(rect, 0);
    if (rect.Contains(pt))
      return (childId && (*childId = id)), (child && (*child = cell)),
             wxACC_OK;
  }

  return (childId && (*childId = 0)), (child && (*child = this)), //-V560
         wxACC_OK;
}

wxAccStatus Cell::GetChild(int childId, wxAccessible **child)
{
  if (!child)
    return wxACC_FAIL;

  if (childId == 0)
    return (*child = this), wxACC_OK;

  if (childId > 0)
    for (auto cell = InnerBegin(); cell != InnerEnd(); ++ cell)
      if (cell && (--childId == 0))
        return (*child = cell), wxACC_OK;

  return wxACC_FAIL;
}

wxAccStatus Cell::GetFocus(int *childId, wxAccessible **child)
{
  int id = 0;
  for (auto cell = InnerBegin(); cell != InnerEnd(); ++cell)
  {
    if (!cell)
      continue;
    ++ id;

    int dummy;
    if (cell->GetFocus(&dummy, child) == wxACC_OK)
      return (childId && (*childId = id)), (child && (*child = cell)),
             wxACC_OK;
  }

  return (childId && (*childId = 0)), (child && (*child = nullptr)), //-V560
         wxACC_FAIL;
}

wxAccStatus Cell::GetLocation(wxRect &rect, int elementId)
{
  if(elementId == 0)
  {
    rect = wxRect(GetRect().GetTopLeft()     + (*m_configuration)->GetVisibleRegion().GetTopLeft(),
                  GetRect().GetBottomRight() + (*m_configuration)->GetVisibleRegion().GetTopLeft());
    if(rect.GetTop() < 0)
      rect.SetTop(0);
    if(rect.GetLeft() < 0)
      rect.SetLeft(0);
    if(rect.GetBottom() > (*m_configuration)->GetVisibleRegion().GetWidth())
      rect.SetBottom((*m_configuration)->GetVisibleRegion().GetWidth());
    if(rect.GetRight() > (*m_configuration)->GetVisibleRegion().GetHeight())
      rect.SetRight((*m_configuration)->GetVisibleRegion().GetHeight());
    rect = wxRect(rect.GetTopLeft()+(*m_configuration)->GetWorksheetPosition(),rect.GetBottomRight()+(*m_configuration)->GetWorksheetPosition());
    return wxACC_OK;
  }

  wxAccessible *cell = NULL;
  if (GetChild(elementId, &cell) == wxACC_OK)
    return cell->GetLocation(rect, 0);

  return wxACC_FAIL;
}

wxAccStatus Cell::GetRole(int WXUNUSED(childId), wxAccRole *role)
{
  if (!role)
    return wxACC_FAIL;

  return (*role = wxROLE_SYSTEM_STATICTEXT), wxACC_OK;
}

#endif

Cell::CellPointers::CellPointers(wxScrolledCanvas *worksheet) :
  m_worksheet(worksheet)
{}

wxString Cell::CellPointers::WXMXGetNewFileName()
{
  wxString file(wxT("image"));
  file << (++m_wxmxImgCounter) << wxT(".");
  return file;
}

Cell::InnerCellIterator Cell::InnerBegin() const { return {}; }
Cell::InnerCellIterator Cell::InnerEnd() const { return {}; }
