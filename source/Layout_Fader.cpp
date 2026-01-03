#include "Layout_Fader.hpp"
#include <cstdio>
#include <ogc/gx.h>

void Layout_Fader::Init()
{
    m_width = 854.0 + 40.0;
    m_height = 480.0 + 40.0;

    m_x = 0.0;
    m_y = 0.0;
}

void Layout_Fader::Calc()
{
    m_animFrame++;

    if (m_animFrame > ANIM_FRAMES) {
        m_animFrame = ANIM_FRAMES;
        m_alpha = m_animState == AnimState::IN ? 0x00 : 0xFF;
    } else if (m_animState == AnimState::IN) {
        m_alpha = 0xFF - (0xFF / ANIM_FRAMES) * m_animFrame;
    } else if (m_animState == AnimState::OUT) {
        m_alpha = (0xFF / ANIM_FRAMES) * m_animFrame;
    }
}

static void SetupGX()
{
    // Set up for drawing f32 quads
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    // Disable texture and indirect
    GX_SetNumTexGens(0);
    GX_SetNumIndStages(0);

    // Set material channels
    GX_SetNumChans(1);
    GX_SetChanCtrl(
        GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE,
        GX_AF_NONE
    );

    // Setup TEV
    GX_SetNumTevStages(1);
    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);

    GX_InvalidateTexAll();

    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);

    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_TRUE);
}

void Layout_Fader::Draw()
{
    if (!m_visible || m_alpha < 1 || m_width == 0 || m_height == 0) {
        return;
    }

    const u8 bgColor = 0;

    SetupGX();

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    {
        // Top left
        GX_Position3f32(m_x - m_width / 2, m_y + m_height / 2, 0.0);
        GX_Color4u8(bgColor, bgColor, bgColor, m_alpha);
        // Top right
        GX_Position3f32(m_x + m_width / 2, m_y + m_height / 2, 0.0);
        GX_Color4u8(bgColor, bgColor, bgColor, m_alpha);
        // Bottom right
        GX_Position3f32(m_x + m_width / 2, m_y - m_height / 2, 0.0);
        GX_Color4u8(bgColor, bgColor, bgColor, m_alpha);
        // Bottom left
        GX_Position3f32(m_x - m_width / 2, m_y - m_height / 2, 0.0);
        GX_Color4u8(bgColor, bgColor, bgColor, m_alpha);
    }
    GX_End();
}
