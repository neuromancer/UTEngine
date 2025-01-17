
#include "Precomp.h"
#include "CoronaRender.h"
#include "RenderDevice/RenderDevice.h"
#include "UObject/UActor.h"
#include "UObject/UTexture.h"
#include "UObject/ULevel.h"
#include "Math/hsb.h"
#include "Engine.h"
#include "Window/Window.h"
#include "UTRenderer.h"

void CoronaRender::DrawCoronas(FSceneNode* frame)
{
	RenderDevice* device = engine->window->GetRenderDevice();

	for (UActor* light : engine->renderer->Lights)
	{
		if (light && light->bCorona() && light->Skin() && !engine->Level->TraceAnyHit(light->Location(), frame->ViewLocation))
		{
			vec4 pos = frame->Modelview * vec4(light->Location(), 1.0f);
			if (pos.z >= 1.0f)
			{
				vec4 clip = frame->Projection * pos;

				float x = frame->FX2 + clip.x / clip.w * frame->FX2;
				float y = frame->FY2 + clip.y / clip.w * frame->FY2;
				float z = 2.0f;

				float width = (float)light->Skin()->Mipmaps.front().Width;
				float height = (float)light->Skin()->Mipmaps.front().Height;
				float scale = frame->FY / 400.0f;

				vec3 lightcolor = hsbtorgb(light->LightHue(), light->LightSaturation(), 255/*light->LightBrightness()*/);

				FTextureInfo texinfo;
				texinfo.CacheID = (uint64_t)(ptrdiff_t)light->Skin();
				texinfo.Texture = light->Skin()->GetAnimTexture();
				device->DrawTile(frame, texinfo, x - width * scale * 0.5f, y - height * scale * 0.5f, width * scale, height * scale, 0.0f, 0.0f, width, height, z, vec4(lightcolor, 1.0f), vec4(0.0f), PF_Translucent);
			}
		}
	}
}
