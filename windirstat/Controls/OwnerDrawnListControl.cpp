// OwnerDrawnListControl.cpp - Implementation of COwnerDrawnListItem and COwnerDrawnListControl
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "WinDirStat.h"
#include "TreeMap.h"    // CColorSpace
#include "SelectObject.h"
#include "OwnerDrawnListControl.h"

namespace
{
    constexpr UINT TEXT_X_MARGIN = 6; // Horizontal distance of the text from the edge of the item rectangle

    constexpr UINT LABEL_INFLATE_CX = 3; // How much the label is enlarged, to get the selection and focus rectangle
    constexpr UINT LABEL_Y_MARGIN = 2;

    constexpr UINT GENERAL_INDENT = 5;
}

/////////////////////////////////////////////////////////////////////////////

// Draws an item label (icon, text) in all parts of the WinDirStat view
// the rest is drawn by DrawItem()
void COwnerDrawnListItem::DrawLabel(const COwnerDrawnListControl* list, CImageList* il, CDC* pdc, CRect& rc, UINT state, int* width, int* focusLeft, bool indent) const
{
    CRect rcRest = rc;
    // Increase indentation according to tree-level
    if (indent)
    {
        rcRest.left += GENERAL_INDENT;
    }

    // Prepare to draw the file/folder icon
    ASSERT(GetImage() < il->GetImageCount());

    IMAGEINFO ii;
    il->GetImageInfo(GetImage(), &ii);
    const CRect rcImage(ii.rcImage);

    if (width == nullptr)
    {
        // Draw the color with transparent background
        const CPoint pt(rcRest.left, rcRest.top + rcRest.Height() / 2 - rcImage.Height() / 2);
        il->SetBkColor(CLR_NONE);
        il->Draw(pdc, GetImage(), pt, ILD_NORMAL);
    }

    // Decrease size of the remainder rectangle from left
    rcRest.left += rcImage.Width();

    CSelectObject sofont(pdc, list->GetFont());

    rcRest.DeflateRect(list->GetTextXMargin(), 0);

    CRect rcLabel = rcRest;
    pdc->DrawText(GetText(0), rcLabel, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_CALCRECT | DT_NOPREFIX);

    rcLabel.InflateRect(LABEL_INFLATE_CX, 0);
    rcLabel.top    = rcRest.top + LABEL_Y_MARGIN;
    rcLabel.bottom = rcRest.bottom - LABEL_Y_MARGIN;

    CSetBkMode bk(pdc, TRANSPARENT);
    COLORREF textColor = ::GetSysColor(COLOR_WINDOWTEXT);
    if (width == nullptr && (state & ODS_SELECTED) != 0 && (list->HasFocus() || list->IsShowSelectionAlways()))
    {
        // Color for the text in a highlighted item (usually white)
        textColor = list->GetHighlightTextColor();

        CRect selection = rcLabel;
        // Depending on "FullRowSelection" style
        if (list->IsFullRowSelection())
        {
            selection.right = rc.right;
        }
        // Fill the selection rectangle background (usually dark blue)
        pdc->FillSolidRect(selection, list->GetHighlightColor());
    }
    else
    {
        // Use the color designated for this item
        // This is currently only for encrypted and compressed items
        textColor = GetItemTextColor();
    }

    // Set text color for device context
    CSetTextColor stc(pdc, textColor);

    if (width == nullptr)
    {
        // Draw the actual text
        pdc->DrawText(GetText(0), rcRest, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_NOPREFIX);
    }

    rcLabel.InflateRect(1, 1);

    *focusLeft = rcLabel.left;

    if ((state & ODS_FOCUS) != 0 && list->HasFocus() && width == nullptr && !list->IsFullRowSelection())
    {
        pdc->DrawFocusRect(rcLabel);
    }

    if (width == nullptr)
    {
        DrawAdditionalState(pdc, rcLabel);
    }

    rcLabel.left = rc.left;
    rc           = rcLabel;

    if (width != nullptr)
    {
        *width = rcLabel.Width() + 5; // Don't know, why +5
    }
}

void COwnerDrawnListItem::DrawSelection(const COwnerDrawnListControl* list, CDC* pdc, CRect rc, UINT state) const
{
    if (!list->IsFullRowSelection())
    {
        return;
    }
    if (!list->HasFocus() && !list->IsShowSelectionAlways())
    {
        return;
    }
    if ((state & ODS_SELECTED) == 0)
    {
        return;
    }

    rc.DeflateRect(0, LABEL_Y_MARGIN);
    pdc->FillSolidRect(rc, list->GetHighlightColor());
}

void COwnerDrawnListItem::DrawPercentage(CDC* pdc, CRect rc, double fraction, COLORREF color) const
{
    constexpr int LIGHT = 198; // light edge
    constexpr int DARK  = 118; // dark edge
    constexpr int BG    = 225; // background (lighter than light edge)

    constexpr COLORREF light = RGB(LIGHT, LIGHT, LIGHT);
    constexpr COLORREF dark  = RGB(DARK, DARK, DARK);
    constexpr COLORREF bg    = RGB(BG, BG, BG);

    CRect rcLeft = rc;
    rcLeft.right = static_cast<int>(rcLeft.left + rc.Width() * fraction);

    CRect rcRight = rc;
    rcRight.left  = rcLeft.right;

    if (rcLeft.right > rcLeft.left)
    {
        pdc->Draw3dRect(rcLeft, light, dark);
    }
    rcLeft.DeflateRect(1, 1);
    if (rcLeft.right > rcLeft.left)
    {
        pdc->FillSolidRect(rcLeft, color);
    }

    if (rcRight.right > rcRight.left)
    {
        pdc->Draw3dRect(rcRight, light, light);
    }
    rcRight.DeflateRect(1, 1);
    if (rcRight.right > rcRight.left)
    {
        pdc->FillSolidRect(rcRight, bg);
    }
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(COwnerDrawnListControl, CSortingListControl)

COwnerDrawnListControl::COwnerDrawnListControl(int rowHeight, std::vector<int>* column_order, std::vector<int>* column_widths)
    : CSortingListControl(column_order, column_widths)
      , m_rowHeight(rowHeight)
{
    ASSERT(rowHeight > 0);
    InitializeColors();
}

// This method MUST be called before the Control is shown.
void COwnerDrawnListControl::OnColumnsInserted()
{
    // The pacman shall not draw over our header control.
    ModifyStyle(0, WS_CLIPCHILDREN);
    LoadPersistentAttributes();
}

void COwnerDrawnListControl::SysColorChanged()
{
    InitializeColors();
}

int COwnerDrawnListControl::GetRowHeight() const
{
    return m_rowHeight;
}

void COwnerDrawnListControl::ShowGrid(bool show)
{
    m_showGrid = show;
    if (::IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void COwnerDrawnListControl::ShowStripes(bool show)
{
    m_showStripes = show;
    if (::IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void COwnerDrawnListControl::ShowFullRowSelection(bool show)
{
    m_showFullRowSelect = show;
    if (::IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

bool COwnerDrawnListControl::IsFullRowSelection() const
{
    return m_showFullRowSelect;
}

// Normal window background color
COLORREF COwnerDrawnListControl::GetWindowColor() const
{
    return m_windowColor;
}

// Shaded window background color (for stripes)
COLORREF COwnerDrawnListControl::GetStripeColor() const
{
    return m_stripeColor;
}

// Highlight color if we have no focus
COLORREF COwnerDrawnListControl::GetNonFocusHighlightColor() const
{
    return RGB(190, 190, 190);
}

// Highlight text color if we have no focus
COLORREF COwnerDrawnListControl::GetNonFocusHighlightTextColor() const
{
    return RGB(0, 0, 0);
}

COLORREF COwnerDrawnListControl::GetHighlightColor() const
{
    if (HasFocus())
    {
        return ::GetSysColor(COLOR_HIGHLIGHT);
    }

    return GetNonFocusHighlightColor();
}

COLORREF COwnerDrawnListControl::GetHighlightTextColor() const
{
    if (HasFocus())
    {
        return ::GetSysColor(COLOR_HIGHLIGHTTEXT);
    }

    return GetNonFocusHighlightTextColor();
}

bool COwnerDrawnListControl::IsItemStripeColor(int i) const
{
    return m_showStripes && i % 2 != 0;
}

bool COwnerDrawnListControl::IsItemStripeColor(const COwnerDrawnListItem* item) const
{
    return IsItemStripeColor(FindListItem(item));
}

COLORREF COwnerDrawnListControl::GetItemBackgroundColor(int i) const
{
    return IsItemStripeColor(i) ? GetStripeColor() : GetWindowColor();
}

COLORREF COwnerDrawnListControl::GetItemBackgroundColor(const COwnerDrawnListItem* item) const
{
    return GetItemBackgroundColor(FindListItem(item));
}

COLORREF COwnerDrawnListControl::GetItemSelectionBackgroundColor(int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection() && (HasFocus() || IsShowSelectionAlways()))
    {
        return GetHighlightColor();
    }

    return GetItemBackgroundColor(i);
}

COLORREF COwnerDrawnListControl::GetItemSelectionBackgroundColor(const COwnerDrawnListItem* item) const
{
    return GetItemSelectionBackgroundColor(FindListItem(item));
}

COLORREF COwnerDrawnListControl::GetItemSelectionTextColor(int i) const
{
    const bool selected = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
    if (selected && IsFullRowSelection() && (HasFocus() || IsShowSelectionAlways()))
    {
        return GetHighlightTextColor();
    }

    return ::GetSysColor(COLOR_WINDOWTEXT);
}

int COwnerDrawnListControl::GetTextXMargin() const
{
    return TEXT_X_MARGIN;
}

int COwnerDrawnListControl::GetGeneralLeftIndent() const
{
    return GENERAL_INDENT;
}

COwnerDrawnListItem* COwnerDrawnListControl::GetItem(int i) const
{
    const auto item = reinterpret_cast<COwnerDrawnListItem*>(GetItemData(i));
    return item;
}

int COwnerDrawnListControl::FindListItem(const COwnerDrawnListItem* item) const
{
    LVFINDINFO fi;
    fi.flags  = LVFI_PARAM;
    fi.lParam = reinterpret_cast<LPARAM>(item);
    return FindItem(&fi);
}

void COwnerDrawnListControl::InitializeColors()
{
    // I try to find a good contrast to COLOR_WINDOW (usually white or light grey).
    // This is a result of experiments.

    constexpr double diff      = 0.07; // Try to alter the brightness by diff.
    constexpr double threshold = 1.04; // If result would be brighter, make color darker.

    m_windowColor = ::GetSysColor(COLOR_WINDOW);

    double b = CColorSpace::GetColorBrightness(m_windowColor);

    if (b + diff > threshold)
    {
        b -= diff;
    }
    else
    {
        b += diff;
        if (b > 1.0)
        {
            b = 1.0;
        }
    }

    m_stripeColor = CColorSpace::MakeBrightColor(m_windowColor, b);
}

void COwnerDrawnListControl::DrawItem(LPDRAWITEMSTRUCT pdis)
{
    const COwnerDrawnListItem* item = reinterpret_cast<COwnerDrawnListItem*>(pdis->itemData);
    CDC* pdc = CDC::FromHandle(pdis->hDC);
    CRect rcItem(pdis->rcItem);

    CDC dc_mem;
    dc_mem.CreateCompatibleDC(pdc);

    CBitmap bm;
    bm.CreateCompatibleBitmap(pdc, rcItem.Width(), rcItem.Height());
    CSelectObject sobm(&dc_mem, &bm);

    dc_mem.FillSolidRect(rcItem - rcItem.TopLeft(),
        GetItemBackgroundColor(static_cast<int>(pdis->itemID)));

    int focusLeft = 0;
    const int header_count = GetHeaderCtrl()->GetItemCount();
    for (int i = 0; i < header_count; i++)
    {
        // The subitem tracks the identifer that maps the column enum
        LVCOLUMN info = { LVCF_SUBITEM };
        GetColumn(i, &info);
        const int subitem = info.iSubItem;

        CRect rc = GetWholeSubitemRect(pdis->itemID, i);
        const CRect rcDraw = rc - rcItem.TopLeft();

        if (!item->DrawSubitem(subitem, &dc_mem, rcDraw, pdis->itemState, nullptr, &focusLeft))
        {
            item->DrawSelection(this, &dc_mem, rcDraw, pdis->itemState);

            CRect rcText = rcDraw;
            rcText.DeflateRect(TEXT_X_MARGIN, 0);
            CSetBkMode bk(&dc_mem, TRANSPARENT);
            CSelectObject sofont(&dc_mem, GetFont());
            const CStringW s = item->GetText(subitem);
            const UINT align = IsColumnRightAligned(subitem) ? DT_RIGHT : DT_LEFT;

            // Get the correct color in case of compressed or encrypted items
            COLORREF textColor = item->GetItemTextColor();

            // Except if the item is selected - in this case just use standard colors
            if (pdis->itemState & ODS_SELECTED && (HasFocus() || IsShowSelectionAlways()) && IsFullRowSelection())
            {
                textColor = GetItemSelectionTextColor(pdis->itemID);
            }

            // Set the text color
            CSetTextColor tc(&dc_mem, textColor);

            // Draw the (sub)item text
            dc_mem.DrawText(s, rcText, DT_SINGLELINE | DT_VCENTER | DT_WORD_ELLIPSIS | DT_NOPREFIX | align);
        }

        if (m_showGrid)
        {
            constexpr COLORREF gridColor = RGB(212, 208, 200);
            CPen pen(PS_SOLID, 1, gridColor);
            CSelectObject sopen(&dc_mem, &pen);

            dc_mem.MoveTo(rcDraw.right, rcDraw.top);
            dc_mem.LineTo(rcDraw.right, rcDraw.bottom);
            dc_mem.MoveTo(rcDraw.left, rcDraw.bottom + 1);
            dc_mem.LineTo(rcDraw.right, rcDraw.bottom + 1);
        }
    }

    if ((pdis->itemState & ODS_FOCUS) != 0 && HasFocus() && IsFullRowSelection())
    {
        CRect focusRect = rcItem - rcItem.TopLeft();
        focusRect.left = focusLeft - 1;
        dc_mem.DrawFocusRect(focusRect);
    }

    pdc->BitBlt(rcItem.left, rcItem.top,
        rcItem.Width(), rcItem.Height(), &dc_mem, 0, 0, SRCCOPY);
}

bool COwnerDrawnListControl::IsColumnRightAligned(int col) const
{
    HDITEM hditem;
    ZeroMemory(&hditem, sizeof(hditem));
    hditem.mask = HDI_FORMAT;

    GetHeaderCtrl()->GetItem(col, &hditem);

    return (hditem.fmt & HDF_RIGHT) != 0;
}

CRect COwnerDrawnListControl::GetWholeSubitemRect(int item, int subitem) const
{
    CRect rc;
    if (subitem == 0)
    {
        // Special case column 0:
        // If we did GetSubItemRect(item 0, LVIR_LABEL, rc)
        // and we have an image list, then we would get the rectangle
        // excluding the image.
        HDITEM hditem = { HDI_WIDTH };
        GetHeaderCtrl()->GetItem(0, &hditem);

        VERIFY(GetItemRect(item, rc, LVIR_LABEL));
        rc.left = rc.right - hditem.cxy;
    }
    else
    {
        VERIFY(GetSubItemRect(item, subitem, LVIR_LABEL, rc));
    }

    if (m_showGrid)
    {
        rc.right--;
        rc.bottom--;
    }
    return rc;
}

bool COwnerDrawnListControl::HasFocus() const
{
    return ::GetFocus() == m_hWnd;
}

bool COwnerDrawnListControl::IsShowSelectionAlways() const
{
    return (GetStyle() & LVS_SHOWSELALWAYS) != 0;
}

int COwnerDrawnListControl::GetSubItemWidth(const COwnerDrawnListItem* item, int subitem)
{
    CClientDC dc(this);
    CRect rc(0, 0, 1000, 1000);

    int width;
    int dummy = rc.left;
    if (item->DrawSubitem(subitem, &dc, rc, 0, &width, &dummy))
    {
        return width;
    }

    const CStringW s = item->GetText(subitem);
    if (s.IsEmpty())
    {
        return 0;
    }

    CSelectObject sofont(&dc, GetFont());
    const UINT align = IsColumnRightAligned(subitem) ? DT_RIGHT : DT_LEFT;
    dc.DrawText(s, rc, DT_SINGLELINE | DT_VCENTER | DT_CALCRECT | DT_NOPREFIX | align);

    rc.InflateRect(TEXT_X_MARGIN, 0);
    return rc.Width();
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(COwnerDrawnListControl, CSortingListControl)
    ON_WM_ERASEBKGND()
    ON_NOTIFY(HDN_DIVIDERDBLCLICK, 0, OnHdnDividerdblclick)
    ON_NOTIFY(HDN_ITEMCHANGING, 0, OnHdnItemchanging)
    ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()
#pragma warning(pop)

BOOL COwnerDrawnListControl::OnEraseBkgnd(CDC* pDC)
{
    ASSERT(GetHeaderCtrl()->GetItemCount() > 0);

    // Calculate bottom of control
    int first_item = 0;
    if (GetItemCount() > 0)
    {
        CRect rc;
        GetItemRect(GetTopIndex(), rc, LVIR_BOUNDS);
        first_item = rc.top;
    }

    const int lineCount = GetCountPerPage() + 1;
    const int firstItem = GetTopIndex();
    const int lastItem = min(firstItem + lineCount, GetItemCount()) - 1;

    ASSERT(GetItemCount() == 0 || firstItem < GetItemCount());
    ASSERT(GetItemCount() == 0 || lastItem < GetItemCount());
    ASSERT(GetItemCount() == 0 || lastItem >= firstItem);

    const int table_bottom = first_item + (lastItem - firstItem + 1) * GetRowHeight();

    // Calculate where the columns end on the right
    int table_right = -GetScrollPos(SB_HORZ);
    for (int i = 0; i < GetHeaderCtrl()->GetItemCount(); i++)
    {
        HDITEM hdi{ HDI_WIDTH };
        GetHeaderCtrl()->GetItem(i, &hdi);
        table_right += hdi.cxy;
    }

    CRect rcClient;
    GetClientRect(rcClient);
    const COLORREF bgcolor = ::GetSysColor(COLOR_WINDOW);

    // draw blank space on right
    CRect fill_right(table_right, rcClient.top, rcClient.right, rcClient.bottom);
    pDC->FillSolidRect(fill_right, bgcolor);

    // draw blank space on bottom
    CRect fill_left(rcClient.left, table_bottom, rcClient.right, rcClient.bottom);
    pDC->FillSolidRect(fill_left, bgcolor);
    
    return true;
}

void COwnerDrawnListControl::OnHdnDividerdblclick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const int subitem = reinterpret_cast<LPNMHEADER>(pNMHDR)->iItem;
    AdjustColumnWidth(subitem);
    *pResult = 0;
}

void COwnerDrawnListControl::AdjustColumnWidth(int col)
{
    int width = 10;
    for (int i = 0; i < GetItemCount(); i++)
    {
        const int w = GetSubItemWidth(GetItem(i), col);
        if (w > width)
        {
            width = w;
        }
    }
    SetColumnWidth(col, width + 5);
}

void COwnerDrawnListControl::OnHdnItemchanging(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    Default();
    InvalidateRect(nullptr);

    *pResult = 0;
}
