
#include "Precomp.h"
#include "CanvasRender.h"
#include "Engine.h"
#include "Window/Window.h"
#include "Package/PackageManager.h"
#include "RenderDevice/RenderDevice.h"
#include "UObject/UFont.h"
#include "UObject/ULevel.h"
#include "UObject/UTexture.h"
#include "UObject/UActor.h"
#include "UObject/UClient.h"
#include "UTRenderer.h"

CanvasRender::CanvasRender()
{
	bigfont = UObject::Cast<UFont>(engine->packages->GetPackage("Engine")->GetUObject("Font", "BigFont"));
	largefont = UObject::Cast<UFont>(engine->packages->GetPackage("Engine")->GetUObject("Font", "LargeFont"));
	medfont = UObject::Cast<UFont>(engine->packages->GetPackage("Engine")->GetUObject("Font", "MedFont"));
	smallfont = UObject::Cast<UFont>(engine->packages->GetPackage("Engine")->GetUObject("Font", "SmallFont"));
}

void CanvasRender::DrawActor(UActor* actor, bool WireFrame, bool ClearZ)
{
	actor->light = engine->renderer->light.FindLightAt(actor->Location(), actor->Region().ZoneNumber);
	actor->bHidden() = false;

	RenderDevice* device = engine->window->GetRenderDevice();
	device->SetSceneNode(&SceneFrame);
	if (ClearZ)
		device->ClearZ(&SceneFrame);
	engine->renderer->mesh.DrawMesh(&SceneFrame, actor/*, WireFrame*/);

	actor->bHidden() = true;
}

void CanvasRender::DrawClippedActor(UActor* actor, bool WireFrame, int X, int Y, int XB, int YB, bool ClearZ)
{
	RenderDevice* device = engine->window->GetRenderDevice();

	int uiscale = engine->renderer->uiscale;

	mat4 rotate = mat4::rotate(radians(-90.0f), 0.0f, 0.0f, -1.0f); // To do: maybe CoordsMatrix is actually wrong?

	FSceneNode frame;
	frame.XB = XB * uiscale;
	frame.YB = YB * uiscale;
	frame.X = X * uiscale;
	frame.Y = Y * uiscale;
	frame.FX = (float)X * uiscale;
	frame.FY = (float)Y * uiscale;
	frame.FX2 = frame.FX * 0.5f;
	frame.FY2 = frame.FY * 0.5f;
	frame.Modelview = SceneRender::CoordsMatrix() * rotate;
	frame.ViewLocation = vec3(0.0f);
	frame.FovAngle = 95.0f;
	float Aspect = frame.FY / frame.FX;
	float RProjZ = (float)std::tan(radians(frame.FovAngle) * 0.5f);
	float RFX2 = 2.0f * RProjZ / frame.FX;
	float RFY2 = 2.0f * RProjZ * Aspect / frame.FY;
	frame.Projection = mat4::frustum(-RProjZ, RProjZ, -Aspect * RProjZ, Aspect * RProjZ, 1.0f, 32768.0f, handedness::left, clipzrange::zero_positive_w);
	device->SetSceneNode(&frame);

	if (ClearZ)
		device->ClearZ(&frame);

	actor->light = vec3(1.0);
	actor->bHidden() = false;
	engine->renderer->mesh.DrawMesh(&frame, actor/*, WireFrame*/);
	actor->bHidden() = true;

	device->SetSceneNode(&SceneFrame);
}

void CanvasRender::DrawTile(UTexture* Tex, float x, float y, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 color, vec4 fog, uint32_t flags)
{
	RenderDevice* device = engine->window->GetRenderDevice();

	Tex = Tex->GetAnimTexture();

	FTextureInfo texinfo;
	texinfo.CacheID = (uint64_t)(ptrdiff_t)Tex;
	texinfo.Texture = Tex;

	if (Tex->bMasked())
		flags |= PF_Masked;

	int uiscale = engine->renderer->uiscale;
	device->DrawTile(&SceneFrame, texinfo, x * uiscale, y * uiscale, XL * uiscale, YL * uiscale, U, V, UL, VL, Z, color, fog, flags);
}

void CanvasRender::DrawTileClipped(UTexture* Tex, float orgX, float orgY, float curX, float curY, float XL, float YL, float U, float V, float UL, float VL, float Z, vec4 color, vec4 fog, uint32_t flags, float clipX, float clipY)
{
	RenderDevice* device = engine->window->GetRenderDevice();

	Tex = Tex->GetAnimTexture();

	FTextureInfo texinfo;
	texinfo.CacheID = (uint64_t)(ptrdiff_t)Tex;
	texinfo.Texture = Tex;

	if (Tex->bMasked())
		flags |= PF_Masked;

	int uiscale = engine->renderer->uiscale;
	Rectf clipBox = Rectf::xywh(orgX, orgY, clipX, clipY);
	Rectf dest = Rectf::xywh(orgX + curX, orgY + curY, XL, YL);
	Rectf src = Rectf::xywh(U, V, UL, VL);
	DrawTile(device, texinfo, dest, src, clipBox, Z, color, fog, flags, uiscale);
}

void CanvasRender::DrawText(UFont* font, vec4 color, float orgX, float orgY, float& curX, float& curY, float& curYL, bool newlineAtEnd, const std::string& text, uint32_t flags, bool center, float spaceX, float spaceY)
{
	RenderDevice* device = engine->window->GetRenderDevice();

	int uiscale = engine->renderer->uiscale;

	float centerX = 0;
	if (center)
		centerX = std::round((engine->canvas->SizeX() - GetTextSize(font, text).x) * 0.5f);

	for (char c : text)
	{
		// To do: word wrap
		// To do: SpaceX and SpaceY also affects DrawText

		if (c == '\n')
		{
			curX = 0;
			curY += curYL;
			curYL = 0;
		}
		else
		{
			FontGlyph glyph = font->GetGlyph(c);

			FTextureInfo texinfo;
			texinfo.CacheID = (uint64_t)(ptrdiff_t)glyph.Texture;
			texinfo.Texture = glyph.Texture;

			int width = glyph.USize;
			int height = glyph.VSize;
			float StartU = (float)glyph.StartU;
			float StartV = (float)glyph.StartV;
			float USize = (float)glyph.USize;
			float VSize = (float)glyph.VSize;

			device->DrawTile(&SceneFrame, texinfo, (orgX + curX + centerX) * uiscale, (float)(orgY + curY) * uiscale, (float)width * uiscale, (float)height * uiscale, StartU, StartV, USize, VSize, 1.0f, color, vec4(0.0f), PF_Highlighted | PF_NoSmooth | PF_Masked);

			curX += width + spaceX;
			curYL = std::max(curYL, (float)glyph.VSize + spaceY);
		}
	}

	if (newlineAtEnd)
	{
		curX = 0;
		curY += curYL;
		curYL = 0;
	}
}

void CanvasRender::DrawTextClipped(UFont* font, vec4 color, float orgX, float orgY, float curX, float curY, const std::string& text, uint32_t flags, bool checkHotKey, float clipX, float clipY, bool center)
{
	RenderDevice* device = engine->window->GetRenderDevice();

	int uiscale = engine->renderer->uiscale;

	FontGlyph uglyph = font->GetGlyph('_');
	int uwidth = uglyph.USize;
	int uheight = uglyph.VSize;
	float uStartU = (float)uglyph.StartU;
	float uStartV = (float)uglyph.StartV;
	float uUSize = (float)uglyph.USize;
	float uVSize = (float)uglyph.VSize;

	Rectf clipBox = Rectf::xywh(orgX, orgY, clipX, clipY);

	float centerX = 0;
	if (center)
		centerX = std::round((clipX - GetTextSize(font, text).x) * 0.5f);

	bool foundAmpersand = false;
	int maxY = 0;
	for (char c : text)
	{
		if (checkHotKey && c == '&' && !foundAmpersand)
		{
			foundAmpersand = true;
		}
		else if (foundAmpersand && c != '&')
		{
			foundAmpersand = false;

			FontGlyph glyph = font->GetGlyph(c);
			if (curX + glyph.USize > (int)clipX)
				break;

			FTextureInfo texinfo;
			texinfo.CacheID = (uint64_t)(ptrdiff_t)glyph.Texture;
			texinfo.Texture = glyph.Texture;

			Rectf dest = Rectf::xywh(orgX + curX + centerX, orgY + curY, (float)glyph.USize, (float)glyph.VSize);
			Rectf src = Rectf::xywh((float)glyph.StartU, (float)glyph.StartV, (float)glyph.USize, (float)glyph.VSize);
			DrawTile(device, texinfo, dest, src, clipBox, 1.0f, color, vec4(0.0f), PF_Highlighted | PF_NoSmooth | PF_Masked, uiscale);

			texinfo.CacheID = (uint64_t)(ptrdiff_t)uglyph.Texture;
			texinfo.Texture = uglyph.Texture;

			dest = Rectf::xywh(orgX + curX + (glyph.USize - uwidth) / 2, orgY + curY, (float)uwidth, (float)uheight);
			src = Rectf::xywh(uStartU, uStartV, uUSize, uVSize);
			DrawTile(device, texinfo, dest, src, clipBox, 1.0f, color, vec4(0.0f), PF_Highlighted | PF_NoSmooth | PF_Masked, uiscale);

			curX += glyph.USize;
			maxY = std::max(maxY, glyph.VSize);
		}
		else
		{
			foundAmpersand = false;

			FontGlyph glyph = font->GetGlyph(c);
			if (curX + glyph.USize > (int)clipX)
				break;

			FTextureInfo texinfo;
			texinfo.CacheID = (uint64_t)(ptrdiff_t)glyph.Texture;
			texinfo.Texture = glyph.Texture;

			Rectf dest = Rectf::xywh(orgX + curX + centerX, orgY + curY, (float)glyph.USize, (float)glyph.VSize);
			Rectf src = Rectf::xywh((float)glyph.StartU, (float)glyph.StartV, (float)glyph.USize, (float)glyph.VSize);
			DrawTile(device, texinfo, dest, src, clipBox, 1.0f, color, vec4(0.0f), PF_Highlighted | PF_NoSmooth | PF_Masked, uiscale);

			curX += glyph.USize;
			maxY = std::max(maxY, glyph.VSize);
		}
	}
}

void CanvasRender::DrawTile(RenderDevice* device, FTextureInfo &texinfo, const Rectf& dest, const Rectf& src, const Rectf& clipBox, float Z, vec4 color, vec4 fog, uint32_t flags, int uiscale)
{
	if (dest.left > dest.right || dest.top > dest.bottom)
		return;

	if (dest.left >= clipBox.left && dest.top >= clipBox.top && dest.right <= clipBox.right && dest.bottom <= clipBox.bottom)
	{
		device->DrawTile(&SceneFrame, texinfo, dest.left * uiscale, dest.top * uiscale, (dest.right - dest.left) * uiscale, (dest.bottom - dest.top) * uiscale, src.left, src.top, src.right - src.left, src.bottom - src.top, Z, color, fog, flags);
	}
	else
	{
		Rectf d = dest;
		Rectf s = src;

		float scaleX = (s.right - s.left) / (d.right - d.left);
		float scaleY = (s.bottom - s.top) / (d.bottom - d.top);

		if (d.left < clipBox.left)
		{
			s.left += scaleX * (clipBox.left - d.left);
			d.left = clipBox.left;
		}
		if (d.right > clipBox.right)
		{
			s.right += scaleX * (clipBox.right - d.right);
			d.right = clipBox.right;
		}
		if (d.top < clipBox.top)
		{
			s.top += scaleY * (clipBox.top - d.top);
			d.top = clipBox.top;
		}
		if (d.bottom > clipBox.bottom)
		{
			s.bottom += scaleY * (clipBox.bottom - d.bottom);
			d.bottom = clipBox.bottom;
		}

		if (d.left < d.right && d.top < d.bottom)
			device->DrawTile(&SceneFrame, texinfo, d.left * uiscale, d.top * uiscale, (d.right - d.left) * uiscale, (d.bottom - d.top) * uiscale, s.left, s.top, s.right - s.left, s.bottom - s.top, Z, color, fog, flags);
	}
}

ivec2 CanvasRender::GetTextClippedSize(UFont* font, const std::string& text, float clipX)
{
	int x = 0;
	int y = 0;
	for (char c : text)
	{
		FontGlyph glyph = font->GetGlyph(c);
		if (x + glyph.USize > (int)clipX)
			break;
		x += glyph.USize;
		y = std::max(y, glyph.VSize);
	}
	return { x, y };
}

ivec2 CanvasRender::GetTextSize(UFont* font, const std::string& text)
{
	int x = 0;
	int y = 0;
	for (char c : text)
	{
		FontGlyph glyph = font->GetGlyph(c);
		x += glyph.USize;
		y = std::max(y, glyph.VSize);
	}
	return { x, y };
}
