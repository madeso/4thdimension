#include "hge.h"
#include "hgeresource.h"
#include "boost/smart_ptr.hpp"
#include "boost/function.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <ctime>
#include <set>
#include <iostream>

#define OS_CURSOR

using namespace boost;
using namespace std;

HGE* hge = 0;
hgeFont* font;
hgeResourceManager* resource=0;

const int WIDTH = 800;
const int HEIGHT = 600;
const float FADE_TIME_INTRO = 2;
const float FADE_TIME_OUTRO = 1;

void Click();

bool gAgainstComputer = true;
bool gBadAi = true;
bool gHardMode = false;

hgeSprite* GetSpriteResource(const std::string& iSpriteName) {
	hgeSprite* sprite = resource->GetSprite(iSpriteName.c_str());
	if( !sprite ) {
		throw iSpriteName;
	}

	return sprite;
}

hgeFont* GetFontResource(const std::string& iFontName) {
	hgeFont* font = resource->GetFont(iFontName.c_str());
	if( !font ) {
		throw iFontName;
	}

	return font;
}

void SetGameCallbacks();
void SetMenuCallbacks();
void BuildRules();

class Label {
public:
	Label(const std::string& iText, float iX, float iY) : x(iX), y(iY), text(iText) {
		font = GetFontResource("uifont");
	}

	void render() {
		float gray = 0.6f;
		font->SetColor( hgeColor(gray, gray, gray, 1).GetHWColor() );
		font->Render(x, y, HGETEXT_LEFT, text.c_str());
	}

	hgeFont* font;
	std::string text;
	float x;
	float y;
};

class Button {
public:
	Button(const std::string& iText, float iX, float iY) : x(iX), y(iY), text(iText), over(false) {
		font = GetFontResource("uifont");
	}

	void render() {
		if( over ) {
			font->SetColor( hgeColor(0, 0, 1, 1).GetHWColor() );
		}
		else {
			font->SetColor( hgeColor(1, 1, 1, 1).GetHWColor() );
		}
		font->Render(x, y, HGETEXT_LEFT, text.c_str());
	}

	bool test() {
		const float w = font->GetStringWidth(text.c_str());
		const float h = font->GetHeight();
		const hgeRect rect(x, y, x+w, y+h);
		float mx=0; float my=0;
		hge->Input_GetMousePos(&mx, &my);
		over = rect.TestPoint(mx, my);
		return over;
	}

	hgeFont* font;
	std::string text;
	float x;
	float y;
	bool over;
};

class Menu {
public:
	Menu() : state(0), cursorDown(false),
		hardAi("Human Versus Hard AI", 250, 200),
		easyAi("Human Versus Easy AI", 250, 250),
		noAi("Human Versus Human", 250, 300),
		aiLabel("Select Gameplay:", 200, 100),
		// ----------------------------------------------
		mostRules("Use most rules", 150, 200),
		mostRulesDesc(" (ignores most trans-square rules)\n"
				  " - recomended for beginners", 200, 250),
		allRules("Use all rules", 150, 350),
		allRulesDesc(" - only for 4d masters", 200, 400),
		rulesLabel("Select how many rules you want to use", 100, 100) {
	}

	void update(float iTime){
		const bool down = hge->Input_KeyDown( HGEK_LBUTTON );
		const bool clicked = !down && cursorDown;
		cursorDown = down;

		if( state==0 ) {
			if( hardAi.test() && clicked ) {
				++state;
				gAgainstComputer = true;
				gBadAi = false;
				Click();
			}
			else if( easyAi.test() && clicked ) {
				++state;
				gAgainstComputer = true;
				gBadAi = true;
				Click();
			}
			else if( noAi.test() && clicked ) {
				++state;
				gAgainstComputer = false;
				gBadAi = false;
				Click();
			}
		}
		else if (state == 1 ) {
			if( allRules.test() && clicked ) {
				state++;
				Click();
				gHardMode = true;
			}
			else if( mostRules.test() && clicked ) {
				state++;
				Click();
				gHardMode = false;
			}
			
		}
		else {
			BuildRules();
			SetGameCallbacks();
			state = 0;
		}
	}

	void render() {
		if( state==0 ) {
			aiLabel.render();
			hardAi.render();
			easyAi.render();
			noAi.render();
		}
		else {
			allRules.render();
			allRulesDesc.render();
			mostRules.render();
			mostRulesDesc.render();
			rulesLabel.render();
		}
	}

	int state;
	bool cursorDown;

	Button hardAi;
	Button easyAi;
	Button noAi;
	Label aiLabel;

	Button allRules;
	Label allRulesDesc;
	Button mostRules;
	Label mostRulesDesc;
	Label rulesLabel;
};

Menu* gMenu = 0;

void ExecuteComputerMove();

struct Object {
	Object() : mShouldKill(false) {
	}
	virtual void render() {
	}

	virtual void update(float delta, bool interact) {
	}

	void kill() {
		mShouldKill = true;
	}
	bool shouldKill() const {
		return mShouldKill;
	}
private:
	bool mShouldKill;
};

struct Background : public Object {
	Background(const std::string& iSpriteName) : sprite(GetSpriteResource(iSpriteName)) {
	}
	void render() {
		sprite->RenderStretch(0, 0, WIDTH, HEIGHT);
	}
	hgeSprite* sprite;
};
struct FullscreenColorSprite : public Object {
	FullscreenColorSprite(const hgeColorRGB& iColor) : color(iColor), sprite(0, 0, 0, WIDTH, HEIGHT) {
	}

	void render() {
		sprite.SetColor(color.GetHWColor());
		sprite.Render(0,0);
	}

	hgeSprite sprite;
	hgeColorRGB color;
};

struct FadeFromBlack : public FullscreenColorSprite {
	FadeFromBlack(float iTime) : FullscreenColorSprite(hgeColorRGB(0,0,0, 1)), time(iTime) {
	}
	void update(float delta, bool interact) {
		color.a -= delta/time;
		if( color.a <= 0.0f ) {
			kill();
		}
		color.Clamp();
	}
	const float time;
};

void QuitGame();
struct FadeToBlackAndExit : public FullscreenColorSprite {
	FadeToBlackAndExit(float iTime) : FullscreenColorSprite(hgeColorRGB(0,0,0, 0)), time(iTime) {
	}
	void update(float delta, bool interact) {
		color.a += delta/time;
		if( color.a > 1.0f ) {
			QuitGame();
		}
		color.Clamp();
	}
	const float time;
};

typedef shared_ptr<Object> ObjectPtr;

bool IsRemoveable(const ObjectPtr& object) {
	return object->shouldKill();
}

struct SuggestedLocation {
	SuggestedLocation() {
		clear();
	}

	void set(float x, float y) {
		valid = true;
		mx = x;
		my = y;
	}
	void clear() {
		valid = false;
	}

	bool get(float* x, float* y, hgeSprite* forSprite) {
		if( valid ) {
			float dx = 0;
			float dy = 0;
			forSprite->GetHotSpot(&dx, &dy);
			*x = mx + dx;
			*y = my + dy;
			return true;
		}
		return false;
	}

private:
	float mx;
	float my;
	bool valid;
};
SuggestedLocation gSuggestedLocation;

enum CrossOrCircle {
	COC_NEITHER,
	COC_CROSS,
	COC_CIRCLE,
	COC_AI
};

CrossOrCircle gCurrentCursorState = COC_CROSS;

void SetCompleteCursor() {
	gCurrentCursorState = COC_NEITHER;
}
void SetStartCursor() {
	gCurrentCursorState = COC_CROSS;
}

void flipCurrentCursorState() {
	if( gCurrentCursorState == COC_CROSS ) {
		if( gAgainstComputer ) {
			gCurrentCursorState = COC_AI;
		}
		else {
			gCurrentCursorState = COC_CIRCLE;
		}
	}
	else {
		gCurrentCursorState = COC_CROSS;
	}
}

struct IconPlacer : public Object {
	IconPlacer() : crossSprite(0) , circleSprite(0)
#ifndef OS_CURSOR
		, cursorSprite(0)
#endif
	{
		crossSprite = GetSpriteResource("CrossSprite");
		circleSprite = GetSpriteResource("CircleSprite");
#ifndef OS_CURSOR
		cursorSprite  = GetSpriteResource("CursorSprite");
#endif
	}

	void render() {
		if( !hge->Input_IsMouseOver() ) return;
		hgeSprite* cursor = crossSprite;

		if( gCurrentCursorState == COC_CIRCLE ) {
			cursor = circleSprite;
		}

		if( gCurrentCursorState == COC_NEITHER ) {
			cursor = 0;
		}

		if( gCurrentCursorState == COC_AI ) {
			cursor = 0; // hourglass ?
		}

		float x = 0;
		float y = 0;
		hge->Input_GetMousePos(&x, &y); // get os cursor position

		float mx = 0;
		float my = 0;
		if( cursor && gSuggestedLocation.get(&mx, &my, cursor) ) { // if a location isn't suggested, the data isn't modified
			cursor->Render(mx, my);
		}

#ifndef OS_CURSOR
		cursorSprite->Render(x, y);
#endif
	}

	hgeSprite* crossSprite;
	hgeSprite* circleSprite;
#ifndef OS_CURSOR
	hgeSprite* cursorSprite;
#endif
};

struct Index {
	Index() : mCube(-1), mCol(-1), mRow(-1) {
	}
	Index(int cube, int col, int row) : mCube(cube), mCol(col), mRow(row) {
	}

	bool operator==(const Index& i) const {
		return mCube == i.mCube &&
			mCol == i.mCol &&
			mRow == i.mRow;
	}
	void operator=(const Index& i) {
		mCube = i.mCube;
		mCol = i.mCol;
		mRow = i.mRow;
	}
	void operator+=(const Index& i) {
		mCube += i.mCube;
		mCol += i.mCol;
		mRow += i.mRow;
	}
	void clear() {
		mCube = -1;
		mCol = -1;
		mRow = -1;
	}

	int mCube;
	int mCol;
	int mRow;
};

class GameWorld;
class WinningCombination {
public:
	Index combination[4];
	CrossOrCircle test(const GameWorld& iWorld) const;
};
typedef boost::function<WinningCombination ()> TestWinningConditionFunction;

class GameWorld {
public:
	GameWorld() : mIsModified(false) {
		clear();
	}
	CrossOrCircle getState(int cube, int col, int row) const {
		return mState[cube][col][row];
	}
	CrossOrCircle getState(const Index& index) const {
		return mState[index.mCube][index.mCol][index.mRow];
	}
	void setState(int cube, int col, int row, CrossOrCircle state) {
		mState[cube][col][row] = state;
		mIsModified = true;
	}
	void clear() {
		for(int i=0; i<4; ++i) {
			for(int j=0; j<4; ++j) {
				for(int k=0; k<4; ++k) {
					mState[i][j][k] = COC_NEITHER;
				}
			}
		}
		mIsModified = true;
	}

	bool canPlacemarker() const {
		for(int i=0; i<4; ++i) {
			for(int j=0; j<4; ++j) {
				for(int k=0; k<4; ++k) {
					if( mState[i][j][k] == COC_NEITHER ) {
						return true;
					}
				}
			}
		}

		return false;
	}

	bool hasBeenModifiedSinceLastCall() {
		bool modified = mIsModified;
		mIsModified = false;
		return modified;
	}

	CrossOrCircle testWinningConditions(WinningCombination* oCombo) const {
		const std::size_t count = mWinningConditions.size();
		for(std::size_t i=0; i<count; ++i) {
			const WinningCombination combo = mWinningConditions[i]();
			const CrossOrCircle result = combo.test(*this);
			if( result != COC_NEITHER ) {
				*oCombo = combo;
				return result;
			}
		}

		return COC_NEITHER;
	}
	void addWinningCondition(TestWinningConditionFunction condition) {
		mWinningConditions.push_back(condition);
	}
	size_t getWinningComditions( vector<WinningCombination>* conditions) {
		const std::size_t count = mWinningConditions.size();
		for(std::size_t i=0; i<count; ++i) {
			const WinningCombination combo = mWinningConditions[i]();
			conditions->push_back(combo);
		}
		return count;
	}

	bool isPlaceFree(int cube, int col, int row) const {
		return mState[cube][col][row] == COC_NEITHER;
	}
	bool isPlaceFree(Index index) const {
		return isPlaceFree(index.mCube, index.mCol, index.mRow);
	}

private:
	CrossOrCircle mState[4][4][4];
	std::vector<TestWinningConditionFunction> mWinningConditions;
	bool mIsModified;
};



Index gCurrent;

CrossOrCircle WinningCombination::test(const GameWorld& iWorld) const {
	CrossOrCircle first = iWorld.getState(combination[0]);
	for(int i=1; i<4; ++i) {
		if( first != iWorld.getState(combination[i]) ) {
			return COC_NEITHER;
		}
	}

	return first;
}

int gWinPulsatingIndex = 0;


void PlaceMarker(GameWorld* world, int cube, int col, int row);

struct Part {
	Part() : extraScale(0), animateIntro(true), direction(1) {
	}

	void setup(GameWorld* iWorld, float* iox, float* ioy, int icube, int r, int c, hgeSprite* iCross, hgeSprite* iCircle) {
		world = iWorld;
		ox = iox;
		oy = ioy;
		cube = icube;
		row = r;
		col = c;
		cross = iCross;
		circle = iCircle;
		extraScale = 0;
	}

	void start() {
		animateIntro = true;
	}

	void render() {
		CrossOrCircle state = world->getState(cube, col, row);
		if( state != COC_NEITHER ) {
			hgeSprite* sprite = 0;

			if( state == COC_CROSS ) {
				sprite = cross;
			}
			else {
				sprite = circle;
			}

			float dx = 0;
			float dy = 0;
			sprite->GetHotSpot(&dx, &dy);
			const float w = sprite->GetWidth();
			const float h = sprite->GetHeight();
			sprite->RenderEx(*ox + col*w+dx, *oy+row*h+dy, 0, 1 + extraScale);
		}
	}

	hgeSprite* getSprite() {
		CrossOrCircle state = world->getState(cube, col, row);
		if( state != COC_NEITHER ) {
			hgeSprite* sprite = 0;

			if( state == COC_CROSS ) {
				sprite = cross;
			}
			else {
				sprite = circle;
			}
			return sprite;
		}
		return 0;
	}

	void update(WinningCombination* combo, float delta) {
		const float speed = 5.0f;

		if( combo ) {
			bool scale = false;
			if( combo ) {
				Index me(cube, col, row);
				if( combo->combination[gWinPulsatingIndex] == me ) {
					scale = true;
				}
			}

			if( !scale ) {
				return;
			}

			const float min = 0.0f;
			const float max = 2.0f;

			extraScale += delta * max * speed * direction * 0.75;
			if( extraScale > max ) {
				extraScale = max;
				direction = -1;
			}
			if( extraScale < min ) {
				extraScale = min;
				direction = 1;
				gWinPulsatingIndex += 1;
				if( gWinPulsatingIndex==4 ) gWinPulsatingIndex=0;
			}
		}
		else {
			hgeSprite* sprite = getSprite();
			if( sprite && animateIntro ) {
				extraScale = 4;
				animateIntro = false;
			}

			if( extraScale > 0 ) {
				extraScale -= delta* 4*speed;
				if( extraScale < 0 ) {
					extraScale = 0;
				}
			}
		}
	}

	void testPlacements(float mx, float my, bool mouse) {
		CrossOrCircle state = world->getState(cube, col, row);
		if( state == COC_NEITHER ) {
			const float w = cross->GetWidth();
			const float h = cross->GetHeight();
			const float rx = *ox + col*w;
			const float ry = *oy+row*h;
			hgeRect rect(rx, ry, rx+w, ry+h);
			const bool over = rect.TestPoint(mx, my);
			Index me(cube, col, row);

			if( mouse && over ) {
				gCurrent = me;
			}

			if( !mouse && over && me==gCurrent ) {
				PlaceMarker(world, cube, col, row);
			}

			if( over ) {
				gSuggestedLocation.set(rx, ry);
			}
		}
	}

	hgeSprite* cross;
	hgeSprite* circle;

	int cube;
	int row;
	int col;

	float* ox;
	float* oy;
	GameWorld* world;

	float extraScale;
	float direction;
	bool animateIntro;
};

void PlaceMarker(GameWorld* world, int cube, int col, int row) {
	if( gCurrentCursorState == COC_CIRCLE ||
		gCurrentCursorState == COC_CROSS ) {
		world->setState(cube, col, row, gCurrentCursorState);
		flipCurrentCursorState();
		gCurrent.clear();
		Click();
	}
}

struct Cube : public Object {
	Cube(float ix, float iy, GameWorld& iWorld, int iCube, WinningCombination** icombo) : x(ix), y(iy), world(iWorld), cube(iCube), combo(icombo) {
		stringstream str;
		str << "Cube_" << iCube+1;
		cubeSprite = GetSpriteResource(str.str().c_str());
		cross = GetSpriteResource("CrossSprite");
		circle = GetSpriteResource("CircleSprite");
		for(int col=0; col<4; ++col) {
			for(int row=0; row<4; ++row) {
				parts[col][row].setup(&world, &x, &y, cube, row, col, cross, circle);
			}
		}
	}

	void render() {
		cubeSprite->Render(x,y);
		for(int col=0; col<4; ++col) {
			for(int row=0; row<4; ++row) {
				parts[col][row].render();
			}
		}
	}

	void start() {
		for(int col=0; col<4; ++col) {
			for(int row=0; row<4; ++row) {
				parts[col][row].start();
			}
		}
	}

	void update(float delta, bool interact) {
		for(int col=0; col<4; ++col) {
			for(int row=0; row<4; ++row) {
				parts[col][row].update(*combo, delta);
			}
		}
		if( interact ) {
			if( hge->Input_IsMouseOver() ) {
				float mx = 0;
				float my = 0;
				hge->Input_GetMousePos(&mx, &my);
				hgeRect rect;
				cubeSprite->GetBoundingBox(x, y, &rect);
				if( rect.TestPoint(mx, my) ) {
					testPlacements(mx, my, hge->Input_GetKeyState(HGEK_LBUTTON));
				}
			}
		}
	}

	void testPlacements(float mx, float my, bool mouse) {
		for(int col=0; col<4; ++col) {
			for(int row=0; row<4; ++row) {
				parts[col][row].testPlacements(mx, my, mouse);
			}
		}
	}

private:
	float x;
	float y;
	hgeSprite* cubeSprite;
	hgeSprite* cross;
	hgeSprite* circle;
	GameWorld& world;
	const int cube;

	WinningCombination** combo;
	Part parts[4][4];
};





struct StartStep {
	StartStep(int iStart, int iStep) : start(iStart), step(iStep) {
	}
	StartStep(int iStep) : start(0), step(iStep) {
		if( iStep < 0 ) start = 3;
	}
	int start;
	int step;
};

struct WinningConditionModular {
	WinningConditionModular(const StartStep& cube, const StartStep& col, const StartStep& row) : start(cube.start, col.start, row.start), step(cube.step, col.step, row.step) {
	}
	WinningCombination operator()() {
		WinningCombination combo;
		Index index = start;
		for(int i=0; i<4; ++i) {
			combo.combination[i] = index;
			index+= step;
		}
		return combo;
	}
	const Index start;
	const Index step;
};

struct PressKeyToContinue : public Object {
	PressKeyToContinue() : time(0) {
	}
	void update(float delta, bool iInteract) {
		interact = !iInteract;
		if( interact ) {
			const float PI = 3.14159265f;
			const float max = 2 * PI;
			const float speed = 4.0f;
			time += delta * speed;
			while( time > max ) {
				time -= max;
			}
		}
		else {
		}
	}
	void render() {
		if( interact ) {
			font->SetColor( hgeColorRGB(1, 1, 1, (sin(time)+1) / 2.0f).GetHWColor() );
			font->printf(WIDTH / 2, HEIGHT - 35, HGETEXT_CENTER, "Click To Play Again");
		}
	}

	float time;
	bool interact;
};
struct Game;
void AddStartNewGameFader(Game& iGame);

struct AiPlacerObject : public Object {
	AiPlacerObject() : thinkTime(0.35) {
	}
	void update(float delta,  bool interact) {
		if( interact ) {
			thinkTime -= delta;
			if( thinkTime < 0 ) {
				kill();
				ExecuteComputerMove();
			}
		}
	}

	float thinkTime;
};

struct Game {
	Game() : quit(false), quiting(false), hasWon(false), combo(0), interactive(true), aiHasMoved(false) {
		add( new Background("BackgroundSprite") );
		for(int i=0; i<4; ++i) {
			add( cube[i] = new Cube(25 + i*193, 48 + i*126, world, i, &combo) );
		}
		add( new PressKeyToContinue() );
		add( new IconPlacer() );
		add( new FadeFromBlack(FADE_TIME_INTRO) );

		const std::string clickEffectName = "Click";
		clickSound = resource->GetEffect(clickEffectName.c_str());
		if( ! clickSound ) throw clickEffectName;

		const std::string cheerEffectName = "Cheer";
		cheerSound = resource->GetEffect(cheerEffectName.c_str());
		if( ! cheerSound ) throw cheerEffectName;
	}
	void buildRules() {
		for(int cube=0; cube<4; ++cube) {
			world.addWinningCondition( WinningConditionModular(StartStep(cube, 0), StartStep(0, 1), StartStep(0,1)) );
			world.addWinningCondition( WinningConditionModular(StartStep(cube, 0), StartStep(0, 1), StartStep(3,-1)) );
			for(int i=0; i<4; ++i) {
				world.addWinningCondition( WinningConditionModular(StartStep(cube, 0), StartStep(i, 0), StartStep(0,1)) );
				world.addWinningCondition( WinningConditionModular(StartStep(cube, 0), StartStep(0, 1), StartStep(i,0)) );
			}
		}
		if( gHardMode ) {
			world.addWinningCondition( WinningConditionModular(StartStep(0, 1), StartStep(0, 1), StartStep(0,1)) );
			world.addWinningCondition( WinningConditionModular(StartStep(0, 1), StartStep(0, 1), StartStep(3,-1)) );

			world.addWinningCondition( WinningConditionModular(StartStep(3, -1), StartStep(0, 1), StartStep(0,1)) );
			world.addWinningCondition( WinningConditionModular(StartStep(3, -1), StartStep(0, 1), StartStep(3,-1)) );
			for(int i=0; i<4; ++i) {
				world.addWinningCondition( WinningConditionModular(StartStep(0, 1), StartStep(i, 0), StartStep(0,1)) );
				world.addWinningCondition( WinningConditionModular(StartStep(0, 1), StartStep(0, 1), StartStep(i,0)) );

				world.addWinningCondition( WinningConditionModular(StartStep(0, 1), StartStep(i, 0), StartStep(3,-1)) );
				world.addWinningCondition( WinningConditionModular(StartStep(0, 1), StartStep(3, -1), StartStep(i,0)) );
			}
		}

		for(int row=0; row<4; ++row) {
			for(int col=0; col<4; ++col) {
				world.addWinningCondition( WinningConditionModular(StartStep(0, 1), StartStep(col, 0), StartStep(row,0)) );
			}
		}
	}
	void render() {
		const std::size_t length = mObjects.size();
		for(std::size_t i=0; i<length; ++i) {
			mObjects[i]->render();
		}
	}

	void clearBoard() {
		world.clear();
		hasWon = false;
		combo = 0;
		for(int c=0; c<4; ++c) cube[c]->start();
	}
	void newGame() {
		SetStartCursor();
		interactive = true;
	}

	void click() {
		hge->Effect_Play(clickSound);
	}

	void cheer() {
		hge->Effect_Play(cheerSound);
	}

	void update(float delta) {
		if( !quiting && hge->Input_GetKeyState(HGEK_ESCAPE)) {
			add( new FadeToBlackAndExit(FADE_TIME_OUTRO) );
			quiting = true;
			//QuitGame();
		}
		gSuggestedLocation.clear();

		static bool oldMouse = hge->Input_GetKeyState(HGEK_LBUTTON);
		const bool mouse = hge->Input_GetKeyState(HGEK_LBUTTON);

		if( interactive && hasWon && mouse==false&& mouse != oldMouse ) {
			AddStartNewGameFader(*this);
			interactive = false;
		}
		const bool interact = !hasWon && interactive;
		const std::size_t length = mObjects.size();
		for(std::size_t i=0; i<length; ++i) {
			mObjects[i]->update(delta, interact);
		}
		mObjects.erase(std::remove_if(mObjects.begin(), mObjects.end(), IsRemoveable),
			mObjects.end() );

		if( interactive && hge->Input_GetKeyState(HGEK_ENTER) ) {
			displayNoWinner();
		}

		if( !mouse ) {
			gCurrent.clear();
		}

		oldMouse = mouse;

		if( world.hasBeenModifiedSinceLastCall() ) {
			WinningCombination combo;
			CrossOrCircle result = world.testWinningConditions(&combo);
			if( result != COC_NEITHER ) {
				displayWinner(combo);
			}
			else if( !world.canPlacemarker() ) {
				displayNoWinner();
			}
		}

		if( interactive && gAgainstComputer ) {
			if( gCurrentCursorState == COC_AI && !aiHasMoved ) {
				add( new AiPlacerObject() );
				aiHasMoved = true;
			}
			else if( gCurrentCursorState != COC_AI ) {
				aiHasMoved = false;
			}
		}
	}

	void displayNoWinner() {
		hasWon = true;
		mWinningCombo = WinningCombination();
		SetCompleteCursor();
		gWinPulsatingIndex = 0;
		combo = 0;
	}

	void displayWinner(const WinningCombination& combo) {
		hasWon = true;
		mWinningCombo = combo;
		SetCompleteCursor();
		gWinPulsatingIndex = 0;
		this->combo = &mWinningCombo;
		cheer();
	}

	void add(Object* iObject) {
		ObjectPtr object(iObject);
		mObjects.push_back(object);
	}
	vector< ObjectPtr> mObjects;
	bool quit;
	bool quiting;
	GameWorld world;
	Cube* cube[4];
	bool hasWon;
	WinningCombination mWinningCombo;
	WinningCombination* combo;
	bool interactive;
	HEFFECT clickSound;
	HEFFECT cheerSound;
	bool aiHasMoved;
};

Game* gGame = 0;

void ComputerPlaceMarker(int cube, int col, int row) {
	gAgainstComputer = false;
	gCurrentCursorState = COC_CIRCLE;
	const bool free = gGame->world.isPlaceFree(cube, col, row);
	assert( free );
	PlaceMarker(&gGame->world, cube, col, row);
	gAgainstComputer = true;
}

float GetWinFactorPlacement(const WinningCombination& combo) {
	float value = 0;
	for(int i=0; i<4; ++i) {
		switch( gGame->world.getState(combo.combination[i]) ) {
			case COC_CIRCLE:
				value += 1;
				break;
			case COC_NEITHER:
				break;
			case COC_CROSS:
				return 0;
			default:
				return -1;
		}
	}

	return value;
}

float GetStopFactorPlacement(const WinningCombination& combo) {
	float value = 1;
	for(int i=0; i<4; ++i) {
		switch( gGame->world.getState(combo.combination[i]) ) {
			case COC_CIRCLE:
				return 0;
				break;
			case COC_CROSS:
				value += 1;
				break;
			case COC_NEITHER:
				break;
			default:
				return -1;
		}
	}

	if( value > 1 ) return value;
	else return 0;
}
/*   */
bool ExecutePlaceWin() {
	vector<WinningCombination> combinations;
	const size_t count = gGame->world.getWinningComditions(&combinations);
	WinningCombination bestCombo;
	float bestFactor = -1;
	for(size_t i=0; i<count; ++i) {
#define FREE(j)	gGame->world.isPlaceFree(combinations[i].combination[j])
		if( FREE(0) || FREE(1) || FREE(2) || FREE(3) ) {
			float factor = GetWinFactorPlacement(combinations[i]) + GetStopFactorPlacement(combinations[i]) ;
			if( bestFactor < 0 || (factor > bestFactor && hge->Random_Int(1,100) < 73) ) {
				bestFactor = factor;
				bestCombo = combinations[i];
			}
		}
#undef FREE
	}

	if( bestFactor > 0 ) {
		int index = 0;
		do {
			index = hge->Random_Int(0, 3);
		} while( !gGame->world.isPlaceFree(bestCombo.combination[index]) );
		Index place = bestCombo.combination[index];
		ComputerPlaceMarker(place.mCube, place.mCol, place.mRow);
		return true;
	}

	return false;
}

int GenerateRandomPlace() {
	return hge->Random_Int(0, 3);
}

void ExecuteRandomPlacement() {
	int cube = -1;
	int col = -1;
	int row = -1;
	do {
		cube = GenerateRandomPlace();
		col = GenerateRandomPlace();
		row = GenerateRandomPlace();
	} while( !gGame->world.isPlaceFree(cube, col, row) );
	ComputerPlaceMarker(cube, col, row);
}

typedef long Factor;

Factor GetPlacingFactor(const Index& i, const vector<WinningCombination>& rules, size_t count) {
	Factor factor = 0;
	for(size_t ruleIndex=0; ruleIndex<count; ++ruleIndex) {
		bool hasMe = false;
		bool hasFree = false;
		for(int placementIndex=0; placementIndex<4; ++placementIndex) {
			if( gGame->world.isPlaceFree(rules[ruleIndex].combination[placementIndex]) ) {
				hasFree = true;
			}
			if( rules[ruleIndex].combination[placementIndex] == i ) {
				hasMe = true;
			}
		}
		if( hasMe && hasFree ) {
			// this place is worth taking a deeper look into 
			int crosses = 0;
			int circles = 0;
			for(int placementIndex=0; placementIndex<4; ++placementIndex) {
				const Index at = rules[ruleIndex].combination[placementIndex];
				switch( gGame->world.getState(at) ) {
					case COC_CIRCLE:
						circles += 1;
						break;
					case COC_CROSS:
						crosses += 1;
						break;
					default:
						break;
				}
			}

			Factor aloneCircles = 0;
			Factor aloneCrosses = 0;
			if( circles == 0 ) {
				aloneCrosses = crosses;
			}
			if( crosses == 0 ) {
				aloneCircles = circles;
			}
			if( aloneCrosses == 3 ) {
				aloneCrosses *= 500;
			}
			if( aloneCrosses >=2 ) {
				aloneCrosses *= aloneCrosses;
			}
			factor += aloneCircles*aloneCircles + aloneCrosses;
		}
	}

	return factor;
}

struct Placement {
	Placement(const vector<WinningCombination>& rules, size_t count, const Index& i) : index(i), factor(0) {
		if( gGame->world.isPlaceFree(index) ) {
			factor = GetPlacingFactor(index, rules, count);
			if( factor < 0 ) {
				int i =0;
				++i;
				i += 9;
			}
		}
		else {
			factor = -1;
		}
	}

	Index index;
	Factor factor;

	bool operator<(const Placement& i) const {
		return factor < i.factor;
	}
};

void ExecuteComputerMoveTest()
{
	multiset<Placement> placements;
	vector<WinningCombination> rules;
	const size_t count = gGame->world.getWinningConditions(&rules);

	for(int cube=0; cube<4; ++cube) {
		for(int col=0; col<4; ++col) {
			for(int row=0; row<4; ++row) {
				Placement placement(rules, count, Index(cube, col, row));
				if( placement.factor >= 0 ) {
					placements.insert(placement);
				}
			}
		}
	}

	// this would indicate that there is nowhere to place and that is handled elsewhere
	assert( !placements.empty() );

	vector<Index> bestPlacements;
	const int bestValue = placements.rbegin()->factor;
	for(multiset<Placement>::iterator i=placements.begin(); i != placements.end(); ++i) {
		if( i->factor == bestValue ) {
			bestPlacements.push_back(i->index);
		}
	}

	const int index = hge->Random_Int(0, bestPlacements.size()-1);
	Index placeThis = bestPlacements[index];
	ComputerPlaceMarker(placeThis.mCube, placeThis.mCol, placeThis.mRow);
}

void ExecuteComputerMove() {
	if( gBadAi ) {
		if( ExecutePlaceWin() ) return;
		ExecuteRandomPlacement();
	}
	else {
		ExecuteComputerMoveTest();
	}
}

void Click() {
	gGame->click();
}

struct StartNewGameFader : public FullscreenColorSprite {
	StartNewGameFader() : FullscreenColorSprite(hgeColorRGB(0,0,0,0)), direction(1) {
	}

	void update(float delta, bool interact) {
		color.a += delta * direction;

		if( color.a > 1 ) {
			gGame->clearBoard();
			direction = -1;
		}
		if( color.a < 0 ) {
			gGame->newGame();
			kill();
		}

	}

	float direction;
};

void AddStartNewGameFader(Game& iGame) {
	iGame.add( new StartNewGameFader() );
}

void QuitGame() {
	gGame->quit = true;
}

bool FrameFunc() {
	gGame->update( hge->Timer_GetDelta() );
	return gGame->quit;
}

bool RenderFunc() {
	hge->Gfx_BeginScene();
	hge->Gfx_Clear(0);
	gGame->render();
	hge->Gfx_EndScene();

	return false;
}

bool MenuFrameFunc() {
	gMenu->update( hge->Timer_GetDelta() );
	return false;
}



bool MenuRenderFunc() {
	hge->Gfx_BeginScene();
	hge->Gfx_Clear(0);
	gMenu->render();
	hge->Gfx_EndScene();

	return false;
}

bool FrameFuncNull() {
	return false;
}
bool RenderFuncNull() {
	return false;
}

void Exit() {
	hge->System_Shutdown();
	hge->Release();
}
void Clear() {
	hge->System_SetState(HGE_FRAMEFUNC, FrameFuncNull);
	hge->System_SetState(HGE_RENDERFUNC, RenderFuncNull);
}

bool YesNo(const std::string& question) {
	return IDYES == MessageBox(0, question.c_str(), "Question!", MB_YESNO | MB_ICONQUESTION | MB_TASKMODAL);
}

int YesNoCansel(const std::string& question) {
	switch(MessageBox(0, question.c_str(), "Question!", MB_YESNOCANCEL | MB_ICONQUESTION | MB_TASKMODAL)) {
		case IDYES:
			return 2;
		case IDNO:
			return 1;
		default:
			return 0;
	}
}

void BuildRules() {
	gGame->buildRules();
}

void SetGameCallbacks() {
	hge->System_SetState(HGE_FRAMEFUNC, FrameFunc);
	hge->System_SetState(HGE_RENDERFUNC, RenderFunc);
}

void SetMenuCallbacks() {
	hge->System_SetState(HGE_FRAMEFUNC, MenuFrameFunc);
	hge->System_SetState(HGE_RENDERFUNC, MenuRenderFunc);
}


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	hge = hgeCreate(HGE_VERSION);

	//hge->System_SetState(HGE_LOGFILE, "4thDimension.log");
	hge->System_SetState(HGE_FRAMEFUNC, MenuFrameFunc);
	hge->System_SetState(HGE_RENDERFUNC, MenuRenderFunc);
	hge->System_SetState(HGE_WINDOWED, true);
	hge->System_SetState(HGE_USESOUND, false);
	hge->System_SetState(HGE_TITLE, "4th Dimension");
	hge->System_SetState(HGE_SCREENWIDTH, WIDTH);
	hge->System_SetState(HGE_SCREENHEIGHT, HEIGHT);
	hge->System_SetState(HGE_SCREENBPP, 32);
	hge->System_SetState(HGE_USESOUND, true);
#ifdef OS_CURSOR
	hge->System_SetState(HGE_HIDEMOUSE, false);
#endif

	hge->Random_Seed( time(0) );

	if( hge->System_Initiate() ) {
		try {
			hgeResourceManager m("resources.res");
			m.Precache();

			resource = &m;
			Game game;
			Menu menu;
			gMenu = &menu;
			gGame = &game;
			
			font = GetFontResource("TheFont");

			const std::string streamFileName = "BackgroundMusic";
			HSTREAM music = m.GetStream(streamFileName.c_str());
			if( !music ) throw streamFileName;
#ifdef NDEBUG
			hge->Stream_Play(music, true);
#endif


			hge->System_Start();
			resource = 0;
			gGame = 0;
			gMenu = 0;
			m.Purge();
		}
		catch(const string& iFile) {
			std::string message = "Failed to load ";
			message += iFile;
			Clear();
			MessageBox(0, message.c_str(), "Error", MB_OK | MB_ICONERROR | MB_TASKMODAL);
			Exit();
			return 0;
		}
	}
	else {
		Clear();
		MessageBox(NULL, hge->System_GetErrorMessage(), "Error", MB_OK | MB_ICONERROR | MB_APPLMODAL);
		Exit();
		return 0;
	}

	Exit();
	return 0;
}