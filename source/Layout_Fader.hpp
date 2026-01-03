#pragma once

#include "Layout.hpp"

class Layout_Fader : public Layout
{
public:
    void Init();
    void Calc() override;
    void Draw() override;

    static constexpr u32 ANIM_FRAMES = 20;

    void StartFadeIn()
    {
        if (m_animState == AnimState::IN) {
            return;
        }

        m_animState = AnimState::IN;
        m_animFrame = 0;
    }

    void StartFadeOut()
    {
        if (m_animState == AnimState::OUT) {
            return;
        }

        m_animState = AnimState::OUT;
        m_animFrame = 0;
    }

    bool IsFadeDone() const
    {
        return m_animFrame >= ANIM_FRAMES;
    }

private:
    enum class AnimState {
        NONE,
        IN,
        OUT,
    };

    AnimState m_animState = AnimState::IN;
    u32 m_animFrame = 0;
};