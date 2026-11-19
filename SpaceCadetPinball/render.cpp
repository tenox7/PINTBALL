#include "pch.h"
#include "render.h"

#include "fullscrn.h"
#include "GroupData.h"
#include "options.h"
#include "pb.h"
#include "score.h"
#include "TPinballTable.h"
#include "winmain.h"
#include "proj.h"

std::vector<render_sprite*> render::sprite_list, render::ball_list;
zmap_header_type* render::background_zmap;
int render::zmap_offsetX, render::zmap_offsetY, render::offset_x, render::offset_y;
rectangle_type render::vscreen_rect;
gdrv_bitmap8 *render::vscreen, *render::background_bitmap, *render::ball_bitmap[20];
zmap_header_type* render::zscreen;
int render::DestX = 0, render::DestY = 0, render::DestWidth = 0, render::DestHeight = 0;
HDC render::BackDC = nullptr;
HBITMAP render::BackBitmap = nullptr, render::BackOldBitmap = nullptr;
ColorRgba* render::BackPixels = nullptr;
int render::BackWidth = 0, render::BackHeight = 0, render::MapSrcWidth = 0, render::MapSrcHeight = 0;
std::vector<int> render::MapX, render::MapY;
RECT render::DirtyRects[MaxDirtyRects];
int render::DirtyCount = 0;
bool render::FullDirty = true, render::ShiftedLast = false;

render_sprite::render_sprite(VisualTypes visualType, gdrv_bitmap8* bmp, zmap_header_type* zMap,
	int xPosition, int yPosition, rectangle_type* boundingRect)
{
	Bmp = bmp;
	ZMap = zMap;
	VisualType = visualType;
	DeleteFlag = false;
	OccludedSprites = nullptr;
	DirtyRect = rectangle_type{};
	DirtyFlag = visualType != VisualTypes::Ball;
	ZMapOffestX = 0;
	ZMapOffestY = 0;
	Depth = 0xffFF;

	if (boundingRect)
	{
		BoundingRect = *boundingRect;
	}
	else
	{
		BoundingRect.Width = -1;
		BoundingRect.Height = -1;
		BoundingRect.XPosition = 0;
		BoundingRect.YPosition = 0;
	}

	BmpRect.XPosition = xPosition;
	BmpRect.YPosition = yPosition;
	if (bmp)
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
	else
	{
		BmpRect.Width = 0;
		BmpRect.Height = 0;
	}
	DirtyRectPrev = BmpRect;

	if (!ZMap && VisualType != VisualTypes::Ball)
	{
		assertm(false, "Background zMap should not be used");
		ZMap = render::background_zmap;
		ZMapOffestY = xPosition - render::zmap_offsetX;
		ZMapOffestX = yPosition - render::zmap_offsetY;
	}

	render::AddSprite(*this);
}

render_sprite::~render_sprite()
{
	render::RemoveSprite(*this);
	delete OccludedSprites;
}

void render_sprite::set(gdrv_bitmap8* bmp, zmap_header_type* zMap, int xPos, int yPos)
{
	if (Bmp == bmp && ZMap == zMap && BmpRect.XPosition == xPos && BmpRect.YPosition == yPos)
		return;

	Bmp = bmp;
	ZMap = zMap;
	DirtyFlag = VisualType != VisualTypes::Ball;
	BmpRect.XPosition = xPos;
	BmpRect.YPosition = yPos;
	if (bmp) 
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
}

void render_sprite::ball_set(gdrv_bitmap8* bmp, float depth, int xPos, int yPos)
{
	set(bmp, ZMap,xPos, yPos);
	Depth = proj::NormalizeDepth(depth);
}

void render::init(gdrv_bitmap8* bmp, int width, int height)
{
	vscreen = new gdrv_bitmap8(width, height, false);
	zscreen = new zmap_header_type(width, height, width);
	zdrv::fill(zscreen, zscreen->Width, zscreen->Height, 0, 0, 0xFFFF);
	vscreen_rect.YPosition = 0;
	vscreen_rect.XPosition = 0;
	vscreen_rect.Width = width;
	vscreen_rect.Height = height;
	vscreen->YPosition = 0;
	vscreen->XPosition = 0;
	for (auto& ballBmp : ball_bitmap)
		ballBmp = new gdrv_bitmap8(64, 64, false);

	background_bitmap = bmp;
	if (bmp)
		gdrv::copy_bitmap(vscreen, width, height, 0, 0, bmp, 0, 0);
	else
		gdrv::fill_bitmap(vscreen, vscreen->Width, vscreen->Height, 0, 0, 0);

	DirtyCount = 0;
	FullDirty = true;
}

void render::uninit()
{
	delete vscreen;
	delete zscreen;

	// Sprite destructor removes it from the list.
	while (!sprite_list.empty())
		delete sprite_list[0];
	while (!ball_list.empty())
		delete ball_list[0];
	for (auto& ballBmp : ball_bitmap)
		delete ballBmp;

	vscreen = nullptr;
	zscreen = nullptr;
	ReleaseBackBuffer();
	DirtyCount = 0;
	FullDirty = true;
	offset_x = offset_y = 0;
	ShiftedLast = false;
}

void render::update()
{
	unpaint_balls();

	// Clip dirty sprites with vScreen, clear clipping (dirty) rectangles
	for (const auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		bool clearSprite = false;
		switch (sprite->VisualType)
		{
		case VisualTypes::Sprite:
			if (sprite->DirtyRectPrev.Width > 0)
				maths::enclosing_box(sprite->DirtyRectPrev, sprite->BmpRect, sprite->DirtyRect);

			if (maths::rectangle_clip(sprite->DirtyRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = true;
			else
				sprite->DirtyRect.Width = -1;
			break;
		case VisualTypes::Background:
			if (maths::rectangle_clip(sprite->BmpRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = !sprite->Bmp;
			else
				sprite->DirtyRect.Width = -1;
			break;
		default: break;
		}

		if (clearSprite)
		{
			auto yPos = sprite->DirtyRect.YPosition;
			auto width = sprite->DirtyRect.Width;
			auto xPos = sprite->DirtyRect.XPosition;
			auto height = sprite->DirtyRect.Height;
			zdrv::fill(zscreen, width, height, xPos, yPos, 0xFFFF);
			if (background_bitmap)
				gdrv::copy_bitmap(vscreen, width, height, xPos, yPos, background_bitmap, xPos, yPos);
			else
				gdrv::fill_bitmap(vscreen, width, height, xPos, yPos, 0);
		}
	}

	// Paint dirty rectangles of dirty sprites
	for (auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		repaint(*sprite);
		sprite->DirtyFlag = false;
		sprite->DirtyRectPrev = sprite->DirtyRect;
		if (sprite->DeleteFlag)
			delete sprite;
	}

	paint_balls();
}

void render::AddSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	list.push_back(&sprite);
}

void render::RemoveSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	auto it = std::find(list.begin(), list.end(), &sprite);
	if (it != list.end())
		list.erase(it);
}

void render::set_background_zmap(zmap_header_type* zMap, int offsetX, int offsetY)
{
	background_zmap = zMap;
	zmap_offsetX = offsetX;
	zmap_offsetY = offsetY;
}

void render::repaint(const render_sprite& sprite)
{
	rectangle_type clipRect{};
	if (!sprite.OccludedSprites || sprite.VisualType == VisualTypes::Ball || sprite.DirtyRect.Width <= 0)
		return;

	for (auto refSprite : *sprite.OccludedSprites)
	{
		if (!refSprite->DeleteFlag && refSprite->Bmp)
		{
			if (maths::rectangle_clip(refSprite->BmpRect, sprite.DirtyRect, &clipRect))
				zdrv::paint(
					clipRect.Width,
					clipRect.Height,
					vscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					zscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					refSprite->Bmp,
					clipRect.XPosition - refSprite->BmpRect.XPosition,
					clipRect.YPosition - refSprite->BmpRect.YPosition,
					refSprite->ZMap,
					clipRect.XPosition + refSprite->ZMapOffestY - refSprite->BmpRect.XPosition,
					clipRect.YPosition + refSprite->ZMapOffestX - refSprite->BmpRect.YPosition);
		}
	}
}


void render::paint_balls()
{
	// Sort ball sprites by ascending depth
	std::sort(ball_list.begin(), ball_list.end(), [](const render_sprite* lhs, const render_sprite* rhs)
	{
		return lhs->Depth < rhs->Depth;
	});

	// For balls that clip vScreen: save original vScreen contents and paint ball bitmap.
	for (auto index = 0u; index < ball_list.size(); ++index)
	{
		auto ball = ball_list[index];
		auto dirty = &ball->DirtyRect;
		if (ball->Bmp && maths::rectangle_clip(ball->BmpRect, vscreen_rect, &ball->DirtyRect))
		{
			int xPos = dirty->XPosition;
			int yPos = dirty->YPosition;
			gdrv::copy_bitmap(ball_bitmap[index], dirty->Width, dirty->Height, 0, 0, vscreen, xPos, yPos);
			zdrv::paint_flat(
				dirty->Width,
				dirty->Height,
				vscreen,
				xPos,
				yPos,
				zscreen,
				xPos,
				yPos,
				ball->Bmp,
				xPos - ball->BmpRect.XPosition,
				yPos - ball->BmpRect.YPosition,
				ball->Depth);
		}
		else
		{
			dirty->Width = -1;
		}
	}
}

void render::unpaint_balls()
{
	// Restore portions of vScreen saved during previous paint_balls call.
	for (int index = static_cast<int>(ball_list.size()) - 1; index >= 0; index--)
	{
		auto curBall = ball_list[index];
		if (curBall->DirtyRect.Width > 0)
			gdrv::copy_bitmap(
				vscreen,
				curBall->DirtyRect.Width,
				curBall->DirtyRect.Height,
				curBall->DirtyRect.XPosition,
				curBall->DirtyRect.YPosition,
				ball_bitmap[index],
				0,
				0);

		curBall->DirtyRectPrev = curBall->DirtyRect;
	}
}

void render::shift(int offsetX, int offsetY)
{
	offset_x += offsetX;
	offset_y += offsetY;
}

void render::build_occlude_list()
{
	std::vector<render_sprite*>* spriteArr = nullptr;
	for (auto mainSprite : sprite_list)
	{
		if (mainSprite->OccludedSprites)
		{
			delete mainSprite->OccludedSprites;
			mainSprite->OccludedSprites = nullptr;
		}

		if (!mainSprite->DeleteFlag && mainSprite->BoundingRect.Width != -1)
		{
			if (!spriteArr)
				spriteArr = new std::vector<render_sprite*>();

			for (auto refSprite : sprite_list)
			{
				if (!refSprite->DeleteFlag
					&& refSprite->BoundingRect.Width != -1
					&& maths::rectangle_clip(mainSprite->BoundingRect, refSprite->BoundingRect, nullptr))
				{
					spriteArr->push_back(refSprite);
				}
			}

			if (mainSprite->Bmp && spriteArr->size() < 2)
				spriteArr->clear();
			if (!spriteArr->empty())
			{
				mainSprite->OccludedSprites = spriteArr;
				spriteArr = nullptr;
			}
		}
	}

	delete spriteArr;
}

// Integer division that rounds up; the values are not negative.
static int CeilDiv(int value, int divisor)
{
	return (value + divisor - 1) / divisor;
}

void render::SetDestination(int x, int y, int width, int height)
{
	DestX = x;
	DestY = y;
	DestWidth = width;
	DestHeight = height;
	FullDirty = true;
}

void render::MarkDirty(int x, int y, int width, int height)
{
	if (FullDirty || !vscreen)
		return;

	RECT rect{x, y, x + width, y + height};
	if (rect.left < 0)
		rect.left = 0;
	if (rect.top < 0)
		rect.top = 0;
	if (rect.right > vscreen->Width)
		rect.right = vscreen->Width;
	if (rect.bottom > vscreen->Height)
		rect.bottom = vscreen->Height;
	if (rect.left >= rect.right || rect.top >= rect.bottom)
		return;

	// Merge with a rectangle that overlaps or touches this one.
	for (int i = 0; i < DirtyCount; i++)
	{
		auto& dirty = DirtyRects[i];
		if (rect.left <= dirty.right && dirty.left <= rect.right &&
			rect.top <= dirty.bottom && dirty.top <= rect.bottom)
		{
			dirty.left = std::min(dirty.left, rect.left);
			dirty.top = std::min(dirty.top, rect.top);
			dirty.right = std::max(dirty.right, rect.right);
			dirty.bottom = std::max(dirty.bottom, rect.bottom);
			return;
		}
	}

	if (DirtyCount < MaxDirtyRects)
	{
		DirtyRects[DirtyCount++] = rect;
	}
	else
	{
		auto& last = DirtyRects[MaxDirtyRects - 1];
		last.left = std::min(last.left, rect.left);
		last.top = std::min(last.top, rect.top);
		last.right = std::max(last.right, rect.right);
		last.bottom = std::max(last.bottom, rect.bottom);
	}
}

bool render::PrepareBackBuffer()
{
	if (!vscreen || DestWidth <= 0 || DestHeight <= 0)
		return false;
	if (BackDC && BackWidth == DestWidth && BackHeight == DestHeight &&
		MapSrcWidth == vscreen->Width && MapSrcHeight == vscreen->Height)
		return true;

	ReleaseBackBuffer();

	// 32-bit top-down DIB: the same pixel layout as ColorRgba.
	BITMAPINFO bmi{};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = DestWidth;
	bmi.bmiHeader.biHeight = -DestHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* bits = nullptr;
	auto screenDC = GetDC(nullptr);
	BackBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
	BackDC = CreateCompatibleDC(screenDC);
	ReleaseDC(nullptr, screenDC);
	if (!BackBitmap || !BackDC || !bits)
	{
		ReleaseBackBuffer();
		return false;
	}

	BackOldBitmap = static_cast<HBITMAP>(SelectObject(BackDC, BackBitmap));
	BackPixels = static_cast<ColorRgba*>(bits);
	BackWidth = DestWidth;
	BackHeight = DestHeight;

	// Nearest neighbor: destination pixel -> source pixel.
	MapSrcWidth = vscreen->Width;
	MapSrcHeight = vscreen->Height;
	MapX.resize(BackWidth);
	MapY.resize(BackHeight);
	for (int x = 0; x < BackWidth; x++)
		MapX[x] = std::min(x * MapSrcWidth / BackWidth, MapSrcWidth - 1);
	for (int y = 0; y < BackHeight; y++)
		MapY[y] = std::min(y * MapSrcHeight / BackHeight, MapSrcHeight - 1);

	FullDirty = true;
	return true;
}

void render::ReleaseBackBuffer()
{
	if (BackDC)
	{
		if (BackOldBitmap)
			SelectObject(BackDC, BackOldBitmap);
		DeleteDC(BackDC);
	}
	if (BackBitmap)
		DeleteObject(BackBitmap);
	BackDC = nullptr;
	BackBitmap = BackOldBitmap = nullptr;
	BackPixels = nullptr;
	BackWidth = BackHeight = MapSrcWidth = MapSrcHeight = 0;
}

void render::ScaleArea(int dstX0, int dstY0, int dstX1, int dstY1)
{
	auto src = vscreen->BmpBufPtr1;
	auto srcStride = vscreen->Stride;
	auto count = dstX1 - dstX0;

	if (BackWidth == vscreen->Width && BackHeight == vscreen->Height)
	{
		for (int y = dstY0; y < dstY1; y++)
			std::memcpy(BackPixels + y * BackWidth + dstX0, src + y * srcStride + dstX0, count * sizeof(ColorRgba));
		return;
	}

	for (int y = dstY0; y < dstY1; y++)
	{
		auto dst = BackPixels + y * BackWidth;
		if (y > dstY0 && MapY[y] == MapY[y - 1])
		{
			// Same source row as the row above.
			std::memcpy(dst + dstX0, dst - BackWidth + dstX0, count * sizeof(ColorRgba));
			continue;
		}

		auto srcRow = src + MapY[y] * srcStride;
		for (int x = dstX0; x < dstX1; x++)
			dst[x] = srcRow[MapX[x]];
	}
}

// Table nudge: the table part of the vscreen moves by the nudge offset; the
// side bar does not move.
void render::ScaleShifted()
{
	auto src = vscreen->BmpBufPtr1;
	auto srcStride = vscreen->Stride;

	auto srcSeparationX = vscreen->Width;
	if (pb::MainTable && pb::MainTable->Width > 0)
		srcSeparationX = std::min(pb::MainTable->Width, vscreen->Width);
	auto dstSeparationX = std::min(CeilDiv(srcSeparationX * BackWidth, vscreen->Width), BackWidth);

	auto offX = static_cast<int>(round(offset_x * fullscrn::ScaleX));
	if (offset_x != 0 && offX == 0)
		offX = Sign(offset_x);
	auto offY = static_cast<int>(round(offset_y * fullscrn::ScaleY));
	if (offset_y != 0 && offY == 0)
		offY = Sign(offset_y);

	const ColorRgba black{0, 0, 0, 0};
	for (int y = 0; y < BackHeight; y++)
	{
		auto dst = BackPixels + y * BackWidth;
		auto srcY = y - offY;
		if (srcY < 0 || srcY >= BackHeight)
		{
			for (int x = 0; x < dstSeparationX; x++)
				dst[x] = black;
		}
		else
		{
			auto srcRow = src + MapY[srcY] * srcStride;
			for (int x = 0; x < dstSeparationX; x++)
			{
				auto srcX = x - offX;
				dst[x] = srcX >= 0 && srcX < dstSeparationX ? srcRow[MapX[srcX]] : black;
			}
		}

		auto sideRow = src + MapY[y] * srcStride;
		for (int x = dstSeparationX; x < BackWidth; x++)
			dst[x] = sideRow[MapX[x]];
	}
}

void render::PresentVScreen(HDC dc)
{
	if (!dc || !PrepareBackBuffer())
		return;

	auto shifted = offset_x != 0 || offset_y != 0;
	if (shifted || ShiftedLast)
		FullDirty = true;
	ShiftedLast = shifted;

	if (FullDirty)
	{
		if (shifted)
			ScaleShifted();
		else
			ScaleArea(0, 0, BackWidth, BackHeight);
		BitBlt(dc, DestX, DestY, BackWidth, BackHeight, BackDC, 0, 0, SRCCOPY);
	}
	else
	{
		for (int i = 0; i < DirtyCount; i++)
		{
			const auto& rect = DirtyRects[i];
			auto x0 = CeilDiv(rect.left * BackWidth, vscreen->Width);
			auto x1 = std::min(CeilDiv(rect.right * BackWidth, vscreen->Width), BackWidth);
			auto y0 = CeilDiv(rect.top * BackHeight, vscreen->Height);
			auto y1 = std::min(CeilDiv(rect.bottom * BackHeight, vscreen->Height), BackHeight);
			if (x0 >= x1 || y0 >= y1)
				continue;

			ScaleArea(x0, y0, x1, y1);
			BitBlt(dc, DestX + x0, DestY + y0, x1 - x0, y1 - y0, BackDC, x0, y0, SRCCOPY);
		}
	}
	GdiFlush();

	DirtyCount = 0;
	FullDirty = false;
}

// WM_PAINT: black borders around the table, then the last frame.
void render::PaintWindow(HDC dc)
{
	RECT client{};
	GetClientRect(WindowFromDC(dc), &client);
	auto black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

	if (!PrepareBackBuffer())
	{
		FillRect(dc, &client, black);
		return;
	}

	if (FullDirty)
	{
		// The image is not current; make it current first.
		PresentVScreen(dc);
	}
	else
	{
		BitBlt(dc, DestX, DestY, BackWidth, BackHeight, BackDC, 0, 0, SRCCOPY);
		GdiFlush();
	}

	RECT border;
	SetRect(&border, client.left, client.top, client.right, DestY);
	FillRect(dc, &border, black);
	SetRect(&border, client.left, DestY + BackHeight, client.right, client.bottom);
	FillRect(dc, &border, black);
	SetRect(&border, client.left, DestY, DestX, DestY + BackHeight);
	FillRect(dc, &border, black);
	SetRect(&border, DestX + BackWidth, DestY, client.right, DestY + BackHeight);
	FillRect(dc, &border, black);
}
