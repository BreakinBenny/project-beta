//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "portal_gamerules.h"
#include "portal_shareddefs.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "engine/IEngineSound.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/Panel.h"
#include "vgui/ISurface.h"
#include "c_portal_player.h"
#include "c_weapon_portalgun.h"
#include "igameuifuncs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	HEALTH_WARNING_THRESHOLD	25


static ConVar	hud_quickinfo("hud_quickinfo", "1", FCVAR_ARCHIVE);
static ConVar	hud_quickinfo_swap("hud_quickinfo_swap", "0", FCVAR_ARCHIVE);

extern ConVar crosshair;
float m_fCBlend1;
float m_fCBlend2;
float BaseLPOpacity;

#define QUICKINFO_EVENT_DURATION	1.0f
#define	QUICKINFO_BRIGHTNESS_FULL	255
#define	QUICKINFO_BRIGHTNESS_DIM	64
#define	QUICKINFO_FADE_IN_TIME		0.5f
#define	QUICKINFO_FADE_OUT_TIME		2.0f
#define BaseLPOpacity	48.0f //Minimum opacity for the last placed portal indicator (What it fades out to)

// I rewrote a lot of this, so I can't comment changes. Comments are important values -- PelPix

/*
==================================================
CHUDQuickInfo
==================================================
*/

using namespace vgui;

class CHUDQuickInfo : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHUDQuickInfo, vgui::Panel);
public:
	CHUDQuickInfo(const char *pElementName);
	void Init(void);
	void VidInit(void);
	bool ShouldDraw(void);
	virtual void OnThink();
	virtual void Paint();

	virtual void ApplySchemeSettings(IScheme *scheme);
private:

	void	DrawWarning(int x, int y, CHudTexture *icon, float &time);
	void	UpdateEventTime(void);
	bool	EventTimeElapsed(void);

	float	m_flLastEventTime;

	float	m_fLastPlacedAlpha[2];
	bool	m_bLastPlacedAlphaCountingUp[2];

	CHudTexture	*m_icon_c;

	CHudTexture	*m_icon_rbn;	// right bracket
	CHudTexture	*m_icon_lbn;	// left bracket

	CHudTexture	*m_icon_rb;		// right bracket, full
	CHudTexture	*m_icon_lb;		// left bracket, full
	CHudTexture	*m_icon_rbe;	// right bracket, empty
	CHudTexture	*m_icon_lbe;	// left bracket, empty

	CHudTexture	*m_icon_rbnone;	// right bracket
	CHudTexture	*m_icon_lbnone;	// left bracket
};

DECLARE_HUDELEMENT(CHUDQuickInfo);

CHUDQuickInfo::CHUDQuickInfo(const char *pElementName) :
CHudElement(pElementName), BaseClass(NULL, "HUDQuickInfo")
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	SetHiddenBits(HIDEHUD_CROSSHAIR);

	m_fLastPlacedAlpha[0] = m_fLastPlacedAlpha[1] = 80;
	m_bLastPlacedAlphaCountingUp[0] = m_bLastPlacedAlphaCountingUp[1] = true;
}

void CHUDQuickInfo::ApplySchemeSettings(IScheme *scheme)
{
	BaseClass::ApplySchemeSettings(scheme);

	SetPaintBackgroundEnabled(false);
}


void CHUDQuickInfo::Init(void)
{
	m_flLastEventTime = 0.0f;
}


void CHUDQuickInfo::VidInit(void)
{
	Init();

	m_icon_c = gHUD.GetIcon("crosshair");

	m_icon_rb = gHUD.GetIcon("crosshair_right_full");
	m_icon_lb = gHUD.GetIcon("crosshair_left_full");
	m_icon_rbe = gHUD.GetIcon("crosshair_right_empty");
	m_icon_lbe = gHUD.GetIcon("crosshair_left_empty");
	m_icon_rbn = gHUD.GetIcon("crosshair_right");
	m_icon_lbn = gHUD.GetIcon("crosshair_left");

}

void CHUDQuickInfo::OnThink()
{
	BaseClass::OnThink();

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if (player == NULL)
		return;

}
void CHUDQuickInfo::DrawWarning(int x, int y, CHudTexture *icon, float &time)
{
	float scale = (int)(fabs(sin(gpGlobals->curtime*8.0f)) * 128.0);

	// Only fade out at the low point of our blink
	if (time <= (gpGlobals->frametime * 200.0f))
	{
		if (scale < 40)
		{
			time = 0.0f;
			return;
		}
		else
		{
			// Counteract the offset below to survive another frame
			time += (gpGlobals->frametime * 200.0f);
		}
	}

	// Update our time
	time -= (gpGlobals->frametime * 200.0f);
	Color caution = gHUD.m_clrCaution;
	caution[3] = scale * 255;

	icon->DrawSelf(x, y, caution);
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHUDQuickInfo::ShouldDraw(void)
{
	if (!m_icon_c || !m_icon_rb || !m_icon_rbe || !m_icon_lb || !m_icon_lbe)
		return false;

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if (player == NULL)
		return false;

	if (!crosshair.GetBool())
		return false;

	return (CHudElement::ShouldDraw() && !engine->IsDrawingLoadingImage());
}


void CHUDQuickInfo::Paint()
{
	C_Portal_Player *pPortalPlayer = (C_Portal_Player*)(C_BasePlayer::GetLocalPlayer());
	if (pPortalPlayer == NULL)
		return;

	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
	if (pWeapon == NULL)
		return;

	int		xCenter = (ScreenWidth() - m_icon_c->Width()) / 2;
	int		yCenter = (ScreenHeight() - m_icon_c->Height()) / 2;

	Color clrNormal = gHUD.m_clrNormal;
	clrNormal[3] = 255;

	SetActive(true);

	m_icon_c->DrawSelf(xCenter, yCenter, clrNormal);

	// adjust center for the bigger crosshairs
	xCenter = ScreenWidth() / 2;
	yCenter = (ScreenHeight() - m_icon_lb->Height()) / 2;

	C_WeaponPortalgun *pPortalgun = dynamic_cast<C_WeaponPortalgun*>(pWeapon);

	// Our colors.
	const unsigned char iAlphaStart = 150;

	Color portal1Color = UTIL_Portal_Color(1);
	Color portal2Color = UTIL_Portal_Color(2);
	if (pPortalgun->IsOlderGun())
	{
		portal2Color = UTIL_Portal_Color(3);
	}
	
	portal1Color[3] = iAlphaStart;
	portal2Color[3] = iAlphaStart;

	const int iBaseLastPlacedAlpha = 128;
	Color lastPlaced1Color = Color(portal1Color[0], portal1Color[1], portal1Color[2], iBaseLastPlacedAlpha);
	Color lastPlaced2Color = Color(portal2Color[0], portal2Color[1], portal2Color[2], iBaseLastPlacedAlpha);

	const float fLastPlacedAlphaLerpSpeed = 300.0f;

	//float flPortalPlacability[2];
	float bPortalPlacability[2];

	// 1 = Can't Place
	// 0 = Can Place
	if (pPortalgun)
	{
		bPortalPlacability[0] = pPortalgun->GetPortal1Placablity();
		bPortalPlacability[1] = pPortalgun->GetPortal2Placablity();
	}

	float fLeftPlaceBarFill = 0.0f;
	float fRightPlaceBarFill = 0.0f;

	if (pPortalgun->CanFirePortal1() && pPortalgun->CanFirePortal2())
	{
		int iDrawLastPlaced = 0;

		//do last placed indicator effects
		if (pPortalgun->GetLastFiredPortal() == 1)
		{
			iDrawLastPlaced = 0;
			fLeftPlaceBarFill = 1.0f;
		}
		else if (pPortalgun->GetLastFiredPortal() == 2)
		{
			iDrawLastPlaced = 1;
			fRightPlaceBarFill = 1.0f;
		}

		if (m_bLastPlacedAlphaCountingUp[iDrawLastPlaced])
		{
			m_fLastPlacedAlpha[iDrawLastPlaced] += gpGlobals->absoluteframetime * fLastPlacedAlphaLerpSpeed * 2.0f;
			if (m_fLastPlacedAlpha[iDrawLastPlaced] > 255.0f)
			{
				m_bLastPlacedAlphaCountingUp[iDrawLastPlaced] = false;
				m_fLastPlacedAlpha[iDrawLastPlaced] = 255.0f - (m_fLastPlacedAlpha[iDrawLastPlaced] - 255.0f);
			}
		}
		else
		{
			m_fLastPlacedAlpha[iDrawLastPlaced] -= gpGlobals->absoluteframetime * fLastPlacedAlphaLerpSpeed;
			if (m_fLastPlacedAlpha[iDrawLastPlaced] < (float)BaseLPOpacity)
			{
				m_fLastPlacedAlpha[iDrawLastPlaced] = (float)BaseLPOpacity;
			}
		}

		//reset the last placed indicator on the other side
		m_fLastPlacedAlpha[1 - iDrawLastPlaced] -= gpGlobals->absoluteframetime * fLastPlacedAlphaLerpSpeed;
		if (m_fLastPlacedAlpha[1 - iDrawLastPlaced] < BaseLPOpacity)
		{
			m_fLastPlacedAlpha[1 - iDrawLastPlaced] = BaseLPOpacity;
		}
		m_bLastPlacedAlphaCountingUp[1 - iDrawLastPlaced] = true;

		if (pPortalgun->GetLastFiredPortal() != 0)
		{
			lastPlaced1Color[3] = m_fLastPlacedAlpha[0];
			lastPlaced2Color[3] = m_fLastPlacedAlpha[1];
		}
		else
		{
			lastPlaced1Color[3] = BaseLPOpacity;
			lastPlaced2Color[3] = BaseLPOpacity;
		}

	}
	//can't fire both portals, and we want the crosshair to remain somewhat symmetrical without being confusing
	else if (!pPortalgun->CanFirePortal1())
	{
		// clone portal2 info to portal 1
		portal1Color = portal2Color;
		lastPlaced1Color[3] = 0.0f;
		lastPlaced2Color[3] = 0.0f;
		bPortalPlacability[0] = bPortalPlacability[1];

	}
	else if (!pPortalgun->CanFirePortal2())
	{
		// clone portal1 info to portal 2
		portal2Color = portal1Color;
		lastPlaced1Color[3] = 0.0f;
		lastPlaced2Color[3] = 0.0f;
		bPortalPlacability[1] = bPortalPlacability[0];
	}

	if (pPortalgun->IsHoldingObject())
	{
		// Change the middle to orange 
		portal1Color = portal2Color = UTIL_Portal_Color(0);

		if (pPortalgun->IsOlderGun() == false)
		{
			if (lastPlaced1Color[3] != 0)
			{
				lastPlaced1Color[0] = UTIL_Portal_Color(0)[0];
				lastPlaced1Color[1] = UTIL_Portal_Color(0)[1];
				lastPlaced1Color[2] = UTIL_Portal_Color(0)[2];
			}

			if (lastPlaced2Color[3] != 0)
			{
				lastPlaced2Color[0] = UTIL_Portal_Color(0)[0];
				lastPlaced2Color[1] = UTIL_Portal_Color(0)[1];
				lastPlaced2Color[2] = UTIL_Portal_Color(0)[2];
			}
		}
		//The last placed portal indicators do not change color in 2006, only the center. --PelPix
		bPortalPlacability[0] = bPortalPlacability[1] = 1.0f;
	}
	/*
	// Note
	1.0f = full
	0.0 = empty
	We need to make the transistions have a delay and not be instant.
	*/

	if (pPortalgun->IsHoldingObject())
	{
		m_fCBlend1 = bPortalPlacability[0];
		m_fCBlend2 = bPortalPlacability[1];
	}
	else
	{
		m_fCBlend1 = Lerp(0.075, m_fCBlend1, bPortalPlacability[0]);
		m_fCBlend2 = Lerp(0.075, m_fCBlend2, bPortalPlacability[1]);
	}

	gHUD.DrawIconProgressBar(xCenter - (m_icon_lb->Width() * 2), yCenter, m_icon_lb, m_icon_lbe, (1.0f - m_fCBlend1), portal1Color, CHud::HUDPB_VERTICAL);
	gHUD.DrawIconProgressBar(xCenter + m_icon_rb->Width(), yCenter, m_icon_rb, m_icon_rbe, (1.0f - m_fCBlend2), portal2Color, CHud::HUDPB_VERTICAL);

	//last placed portal indicator
	if (pPortalgun->CanFirePortal1() && pPortalgun->CanFirePortal2())
	{
		if (pPortalgun->GetLastFiredPortal() == 1)
		{
			m_icon_lb->DrawSelf(xCenter - (m_icon_lb->Width() * 2.60f), yCenter, lastPlaced1Color); //2.75f
		}
		else
		{
			m_icon_lbe->DrawSelf(xCenter - (m_icon_lbe->Width() * 2.60f), yCenter, lastPlaced1Color);//2.75f
		}

		if (pPortalgun->GetLastFiredPortal() == 2)
		{
			m_icon_rb->DrawSelf(xCenter + (m_icon_rb->Width() * 1.60f), yCenter, lastPlaced2Color); //1.75f
		}
		else
		{
			m_icon_rbe->DrawSelf(xCenter + (m_icon_rbe->Width() * 1.60f), yCenter, lastPlaced2Color);//1.75f
		}
	}
	else if (pPortalgun->CanFirePortal1() && !pPortalgun->CanFirePortal2())
	{
		m_icon_lbe->DrawSelf(xCenter - (m_icon_lbe->Width() * 2.60f), yCenter, portal1Color);//2.75f
		m_icon_rbe->DrawSelf(xCenter + (m_icon_rbe->Width() * 1.60f), yCenter, portal1Color);//1.75f

	}
	else if (!pPortalgun->CanFirePortal1() && pPortalgun->CanFirePortal2())
	{
		m_icon_lbe->DrawSelf(xCenter - (m_icon_lbe->Width() * 2.60f), yCenter, portal2Color);//2.75f
		m_icon_rbe->DrawSelf(xCenter + (m_icon_rbe->Width() * 1.60f), yCenter, portal2Color);//1.75f
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHUDQuickInfo::UpdateEventTime(void)
{
	m_flLastEventTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHUDQuickInfo::EventTimeElapsed(void)
{
	if ((gpGlobals->curtime - m_flLastEventTime) > QUICKINFO_EVENT_DURATION)
		return true;

	return false;
}