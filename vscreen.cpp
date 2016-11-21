///////////////////////////////////////////////////////////////
//
// vscreen: Off-Screen Screen (RGBA Sprite 2D Rendering)
//
//
//   19.04.2013 14:08 - created [extracted from main.cpp]
//   29.05.2013 11:56 - added support to render partially off-screen sprites
//   30.05.2013 10:30 - fixed off-screen sprite rendering bug
//

// (partial source) //

void VScreen::Clear(DWORD color)
{
    DWORD* ptr;
    DWORD* ptrEnd;

    ::GdiFlush();

    ptr = this->surfScr.pData;
    ptrEnd = ptr + (this->surfScr.height * this->surfScr.width);
    while(ptr < ptrEnd)
    {
        *ptr = color;
        ptr++;
    }
}

void VScreen::RenderBegin(void)
{
    ::GdiFlush();
}

void VScreen::RenderQuad(Quad* pQuad, DWORD color)
{
    DWORD* ptr;
    DWORD* ptrLineEnd;
    DWORD* ptrEnd;
    DWORD* ptrStop;

    // ::GdiFlush() is assumed

    ptr = this->surfScr.pData + (pQuad->y * this->surfScr.width) + pQuad->x;
    ptrLineEnd = ptr + pQuad->width;
    ptrStop = this->surfScr.pData + (this->surfScr.height * this->surfScr.width);
    ptrEnd = ptr + ((pQuad->height - 1) * this->surfScr.width) + pQuad->width;
    if(ptrEnd > ptrStop)
    {
        ptrEnd = ptrStop;
    }
    while(ptr < ptrEnd)
    {
        if(ptr < ptrLineEnd)
        {
            *ptr = color;
            ptr++;
        }
        else
        {
            // one line lower, step back by rect-width
            ptr += this->surfScr.width - pQuad->width;
            // advance line end
            ptrLineEnd += this->surfScr.width;
        }
    }
}

void VScreen::BlitARGB(Surface* pDstSurface, INT dstX, INT dstY, Surface* pSrcSurface, UINT srcX, UINT srcY, DWORD blitWidth, DWORD blitHeight)
{
    DWORD* ptrSrc;
    DWORD* ptrDst;
    DWORD* ptrDstLineBegin;
    DWORD* ptrDstLineEnd;
    DWORD* ptrSrcLineEnd;
    DWORD* ptrSrcEnd;
    DWORD* ptrDstEnd;
    DWORD* ptrSrcStop;
    DWORD* ptrDstStop;
    ARGB color;
    BOOL bNextDstLine;

    // ::GdiFlush() is assumed

    // adjust some pointers
    ptrSrc = pSrcSurface->pData + (srcY * pSrcSurface->width) + srcX;
    ptrDst = pDstSurface->pData + (dstY * pDstSurface->width) + dstX;
    ptrDstLineBegin = pDstSurface->pData + (dstY * pDstSurface->width);
    if(ptrDstLineBegin < pDstSurface->pData)
    {
        ptrDstLineBegin = pDstSurface->pData;
    }
    ptrDstLineEnd = ptrDstLineBegin + pDstSurface->width;
    ptrSrcLineEnd = ptrSrc + blitWidth;
    ptrSrcStop = pSrcSurface->pData + (pSrcSurface->height * pSrcSurface->width);
    ptrDstStop = pDstSurface->pData + (pDstSurface->height * pDstSurface->width);
    ptrSrcEnd = ptrSrc + ((blitHeight-1) * pSrcSurface->width) + blitWidth;
    ptrDstEnd = ptrDst + ((blitHeight-1) * pDstSurface->width) + blitWidth;
    if(ptrSrcEnd > ptrSrcStop)
    {
        ptrSrcEnd = ptrSrcStop;
    }
    if(ptrDstEnd > ptrDstStop)
    {
        ptrDstEnd = ptrDstStop;
    }
    // perform line-by-line block transfer
    bNextDstLine = FALSE;
    while((ptrSrc < ptrSrcEnd) && (ptrDst < ptrDstEnd))
    {
        if(ptrSrc < ptrSrcLineEnd)
        {
            // skip off-screen pixels
            if((ptrDst >= ptrDstLineBegin) && (ptrDst < ptrDstLineEnd))
            {
                // skip transparent pixels
                color.dword = *ptrSrc;
                if(color.alpha == 0xFF)
                {
                    *ptrDst = color.dword;
                }
                bNextDstLine = TRUE;
            }
            ptrSrc++;
            ptrDst++;
        }
        else
        {
            // one line lower, step back by blit-width
            ptrSrc += pSrcSurface->width - blitWidth;
            ptrDst += pDstSurface->width - blitWidth;
            // advance line boundaries
            ptrSrcLineEnd += pSrcSurface->width;
            if(bNextDstLine != FALSE)
            {
                ptrDstLineBegin += pDstSurface->width;
                ptrDstLineEnd += pDstSurface->width;
                bNextDstLine = FALSE;
            }
        }
    }
}

void VScreen::RenderSprite(DWORD type, INT x, INT y)
{
    Quad* pst;
    pst = &this->spriteTex[type];
///    this->BlitARGB(this->pbm, x, y, this->scrWidth, this->scrHeight, this->ptex, pst->x, pst->y, this->texWidth, this->texHeight, pst->width, pst->height);
    this->BlitARGB(&this->surfScr, x, y, &this->surfTex, pst->x, pst->y, pst->width, pst->height);
}

void VScreen::RenderEnd(void)
{
    // do-nothing
}

void VScreen::PresentTo(HDC hdc)
{
    ::BitBlt(hdc, 0, 0, this->surfScr.width, this->surfScr.height, this->hdc, 0, 0, SRCCOPY);
}

