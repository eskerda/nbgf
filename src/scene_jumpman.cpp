#include <sstream>

#include "scene_jumpman.h"
#include "scene_manager.h"
#include "input.h"
#ifdef _IMGUI
#include "imgui.h"
#endif
#include "bullet.h"
#include "enemy_door.h"
#include "assets.h"
#include "parallax.h"
#include "bat.h"
#include "missile.h"
#include "mantis.h"
#include "bipedal.h"
#include "fx.h"
#include "fireslime.h"
#include "fireshot.h"
#include "explosive.h"
#include "lava.h"
#include "savestation.h"
#include "common_enemy.h"
#include "debug.h"
#include "minotaur.h"
#include "collide.h"
#include "rototext.h"
#include "goomba.h"
#include "flyingalien.h"
#include "drawall.h"
#include "savestate.h"
#include "bigitem.h"
#include "rocketlauncher.h"
#include "health.h"
#include "healthup.h"
#include "tiled_objects_areas.h"
#include "tiled_tilemap.h"
#include "tiled_objects_entities.h"
#include "tiled_objects_screens.h"

extern float mainClock;

const float kCamSpeed = 2000.f;
const float kCamZoomSpeed = 0.2f;

const char* kSaveStateGameName = "gaem2020";

JumpScene::JumpScene(int saveSlot)
	: map(Tiled::TileMap::Size.x, Tiled::TileMap::Size.y, Assets::spritesheetTexture)
	, rotoText(Assets::font_30, Assets::font_30_outline)
	, fogPartSys(Assets::fogTexture)
	, saveSlot(saveSlot)
{
	Bullet::InitParticles();
	Missile::InitParticles();
	Health::InitParticles();

	fogPartSys.AddSprite({ 0, 0, 140, 61 });
	fogPartSys.AddSprite({140, 0, 140, 61 });
	fogPartSys.AddSprite({140*2, 0, 140, 61 });
	fogPartSys.AddSprite({140 * 3, 0, 140, 61 });
	fogPartSys.alpha = 0.f;
	fogPartSys.alpha_vel = 0.1f;
	fogPartSys.bounce_alpha = 0.5f;
	fogPartSys.min_scale = 2.f;
	fogPartSys.max_scale = 3.f;
	fogPartSys.min_vel.x = 17.f;
	fogPartSys.max_vel.x = 20.f;
	fogPartSys.min_ttl = fogPartSys.max_ttl = 10.f;
	fogPartSys.min_interval = 4.5f;
	fogPartSys.max_interval = 6.f;

	for (const BoxBounds& b : Tiled::Areas::lava_bg) {
		new Parallax(b, Assets::lavaParallaxTextures, 0.3f, 1.f, -410.f);
	}

	for (const BoxBounds& b : Tiled::Areas::parallax_forest) {
		new Parallax(b, Assets::forestParallaxTextures, 0.25f, 1.f, 172.f);
	}

	for (const BoxBounds& b : Tiled::Areas::parallax_island) {
		new Parallax(b, Assets::islandParallaxTextures, 0.f, 0.3f, -88.9f);
	}

	for (const BoxBounds& b : Tiled::Areas::parallax_cave) {
		new Parallax(b, Assets::caveParallaxTextures, 0.4f, 0.65f, -165.f);
	}

	for (const auto& screen : Tiled::Screens::screen) {
		screenManager.AddScreen(screen);
	}
}

JumpScene::~JumpScene() {
	Parallax::DeleteAll();
	screenManager.DeleteAllScreens();
}

void JumpScene::SaveGame() const {
	SaveState saveState = SaveState::Open(kSaveStateGameName, saveSlot);
	if (saveState.HasData()) {
		Debug::out << "Overwriting data in slot " << saveSlot;
	}
	saveState.Clear();

	player.SaveGame(saveState);

	for (HealthUp* g : HealthUp::GetAll()) {
		g->SaveGame(saveState);
	}

	for (EnemyDoor* g : EnemyDoor::GetAll()) {
		g->SaveGame(saveState);
	}

	for (Explosive* g : Explosive::GetAll()) {
		g->SaveGame(saveState);
	}

	skillTree.SaveGame(saveState);
	destroyedTiles.SaveGame(saveState);

	saveState.StreamPut("bossdead_bipedal") << (boss_bipedal == nullptr);
	saveState.StreamPut("bossdead_mantis") << (Mantis::GetAll().empty());

	saveState.Save();
}

void JumpScene::LoadGame() {
	SaveState saveState = SaveState::Open(kSaveStateGameName, saveSlot);
	if (!saveState.HasData()) {
		Debug::out << "No data to load in slot " << saveSlot;
		return;
	}

	for (HealthUp* g : HealthUp::GetAll()) {
		g->LoadGame(saveState);
	}

	for (EnemyDoor* g : EnemyDoor::GetAll()) {
		g->LoadGame(saveState);
	}

	for (Explosive* g : Explosive::GetAll()) {
		g->LoadGame(saveState);
	}

	destroyedTiles.LoadGame(saveState);
	skillTree.LoadGame(saveState);

	for (BigItem* g : BigItem::GetAll()) {
		if (skillTree.IsEnabled(g->skill)) {
			TriggerPickupItem(g, true);
			g->alive = false;
		}
	}

	bool bossdead_bipedal = false;
	saveState.StreamGet("bossdead_bipedal") >> bossdead_bipedal;
	if (bossdead_bipedal && boss_bipedal) {
		boss_bipedal->alive = false;
	}


	bool bossdead_mantis = false;
	saveState.StreamGet("bossdead_mantis") >> bossdead_mantis;
	if (bossdead_mantis) {
		for (auto m : Mantis::GetAll()) {
			m->alive = false;
		}
	}
	
	player.LoadGame(saveState);

	Camera::SetCenter(player.GetCameraTargetPos()); // Needed so Update doesn't return because we are on a "camera transition"
	Update(0); //Hackish way to make sure the entities with alive=false trigger things on other components before being deleted
}

void JumpScene::TriggerPickupItem(BigItem* g, [[maybe_unused]] bool fromSave) {

	switch (g->skill) {
	case Skill::WALLJUMP:
	{
	}
	break;
	case Skill::ATTACK:
	{
		int screen_gun = screenManager.FindScreenContaining(g->pos);
		for (Bat* e : Bat::GetAll()) {
			if (e->screen == screen_gun) {
				e->awakened = true;
			}
		}
		for (auto const& [id, pos] : Tiled::Entities::initial_batawake) {
			Bat* b = new Bat(pos, false, true);
			door_to_close_when_break_skill->AddEnemy(b);
		}
	}
	break;
	case Skill::BREAK:
	{
		door_to_close_when_break_skill->Lock();
	}
	break;
	default:
		break;
	}
}

void JumpScene::EnterScene()
{
	player.Reset(Tiled::Entities::single_spawn);
	skillTree.Reset();

	map.LoadFromTiled<Tiled::TileMap>();

	new BigItem(Tiled::Entities::single_skill_walljump, Skill::WALLJUMP);
	new BigItem(Tiled::Entities::single_skill_gun, Skill::GUN);
	BigItem* break_skill = new BigItem(Tiled::Entities::single_skill_breakblocks, Skill::BREAK);
	new BigItem(Tiled::Entities::single_skill_attack, Skill::ATTACK);
	new BigItem(Tiled::Entities::single_skill_dive, Skill::DIVE);
	new BigItem(Tiled::Entities::single_skill_dash, Skill::DASH);

	int screen_break_skill = screenManager.FindScreenContaining(break_skill->pos);

	for (auto const& [id, pos] : Tiled::Entities::save) {
		new SaveStation(id, pos);
	}

	for (auto const& [id, pos] : Tiled::Entities::enemy_door) {
		EnemyDoor* d = new EnemyDoor(id, pos);
		int door_screen = screenManager.FindScreenContaining(d->pos);
		if (door_screen == screen_break_skill) {
			door_to_close_when_break_skill = d;
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::bat) {
		Bat* b = new Bat(pos, false, false);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::angrybat) {
		Bat* b = new Bat(pos, true, false);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::angrybatawake) {
		Bat* b = new Bat(pos, true, true);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::batawake) {
		Bat* b =new Bat(pos, false, true);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::fireslime) {
		auto b = new FireSlime(pos);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::rocket_launcher) {
		new RocketLauncher(pos);
	}

	for (auto const& [id, pos] : Tiled::Entities::goomba) {
		auto b = new Goomba(pos, false);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::goombacharger) {
		auto b = new Goomba(pos,true);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::minotaur) {
		auto b = new Minotaur(pos);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::mantis) {
		auto b = new Mantis(pos);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
		for (SaveStation* s : SaveStation::ByScreen[b->screen]) {
			s->AddHiddenBy(b);
		}
	}

	for (auto const& [id, pos] : Tiled::Entities::flyingalien) {
		auto b = new FlyingAlien(pos);
		for (EnemyDoor* s : EnemyDoor::ByScreen[b->screen]) {
			s->AddEnemy(b);
		}
	}

	Bipedal* bipedal = new Bipedal(Tiled::Entities::single_boss_bipedal);
	for (EnemyDoor* s : EnemyDoor::ByScreen[bipedal->screen]) {
		s->AddEnemy(bipedal);
	}
	boss_bipedal = bipedal;
	for (SaveStation* s : SaveStation::ByScreen[screenManager.FindScreenContaining(boss_bipedal->pos)]) {
		s->AddHiddenBy(boss_bipedal);
	}

	for (auto const& [id, pos] : Tiled::Entities::healthup) {
		new HealthUp(id, pos);
	}

	for (auto const& [id, pos] : Tiled::Entities::explosive) {
		new Explosive(id, pos, false);
	}

	for (auto const& [id, pos] : Tiled::Entities::temp_explosive) {
		new Explosive(id, pos, true);
	}

	for (const BoxBounds& a : Tiled::Areas::lava) {
		Lava* lava = new Lava(a);
		if (a.Contains(Tiled::Entities::single_lava_initial_height)) {
			raising_lava = lava;
			raising_lava_target_height = lava->CurrentLevel();
			lava->SetLevel(Tiled::Entities::single_lava_initial_height.y, true);
		}
	}

	Camera::SetCenter(player.GetCameraTargetPos());

	LoadGame();

	Fx::FreezeImage::SetAlternativeUpdateFnWhileFrozen([this](float dt){
		UpdateCamera(dt);
	});

	Fx::ScreenTransition::Start(Assets::fadeInDiamondsShader);
}

bool JumpScene::UpdateCamera(float dt) {
	float camZoom = Fx::FreezeImage::IsFrozen() && player.justHit ? 1.5f : 1.f;
	float oldZoom = Camera::Zoom();
	float zoomChange = camZoom - oldZoom;
	Mates::Clamp(zoomChange, -kCamZoomSpeed *dt, kCamZoomSpeed *dt);
	Camera::SetZoom(oldZoom + zoomChange);

	vec camPos = player.GetCameraTargetPos();
	vec oldPos = Camera::Center();
	vec displacement = camPos - oldPos;
	bool inScreenTransition = displacement.Truncate(kCamSpeed * dt);
	Camera::SetCenter(oldPos + displacement);
	return inScreenTransition;
}

void JumpScene::ExitScene()
{
	Bullet::DeleteAll();
	Bullet::particles.Clear();
	Missile::DeleteAll();
	Missile::particles.Clear();
	FireShot::DeleteAll();
	Bat::DeleteAll();
	Goomba::DeleteAll();
	RocketLauncher::DeleteAll();
	Minotaur::DeleteAll();
	Mantis::DeleteAll();
	FlyingAlien::DeleteAll();
	FireSlime::DeleteAll();
	OneShotAnim::DeleteAll();
	Bipedal::DeleteAll();
	Lava::DeleteAll();
	destroyedTiles.Clear();
	EnemyDoor::DeleteAll();
	BigItem::DeleteAll();
	HealthUp::DeleteAll();
	Explosive::DeleteAll();
	Health::DeleteAll();
	Health::particles.Clear();
	SaveStation::DeleteAll();
}

void JumpScene::Update(float dt)
{
#ifdef _DEBUG
	if (Debug::FrameByFrame && Debug::Draw && Keyboard::IsKeyPressed(SDL_SCANCODE_LSHIFT)) {
		player.pos = Camera::Center()+vec(0,16);
	}
#endif

	if (Fx::ScreenTransition::IsJustFinished()) {
		if (Fx::ScreenTransition::Current() != &Assets::fadeInDiamondsShader) {
			// This was a death or outro transition: restart scene
			SceneManager::RestartScene();
		}
		return;
	}

	if (player.health <= 0) {
		//Assets::soundDeath.Play();
		vec normalizedPlayerPos = Camera::WorldToScreen(player.Bounds().Center()) / Camera::InScreenCoords::Size();
		Assets::fadeOutCircleShader.Activate(); // Must be active to set uniforms
		Assets::fadeOutCircleShader.SetUniform("normalizedTarget", normalizedPlayerPos);
		Shader::Deactivate();
		Fx::ScreenTransition::Start(Assets::fadeOutCircleShader);
		return;
	}

	contextActionButton = false;

	for (OneShotAnim* e : OneShotAnim::GetAll()) { // Update this first so one-frame anims aren't deleted before they are drawn once
		e->Update(dt);
	}
	OneShotAnim::DeleteNotAlive();

	player.Update(dt);

	screenManager.UpdateCurrentScreen(player.pos);

	bool inScreenTransition = UpdateCamera(dt);
	if (inScreenTransition) {
		return;
	}

	for (RocketLauncher* e : RocketLauncher::GetAll()) {
		e->Update(dt);
	}

	for (Bipedal* e : Bipedal::GetAll()) {
		e->Update(dt);
	}

	for (Bullet* e  : Bullet::GetAll()) {
		e->Update(dt);
	}
	Bullet::DeleteNotAlive();

	for (FireShot* e : FireShot::GetAll()) {
		e->Update(dt);
	}
	FireShot::DeleteNotAlive();
	
	for (Missile* e  : Missile::GetAll()) {
		e->Update(dt);
	}
	Missile::DeleteNotAlive();

	for (Bat* e : Bat::GetAll()) {
		e->Update(dt);
	}

	for (Goomba* e : Goomba::GetAll()) {
		e->Update(dt);
	}

	for (Minotaur* e : Minotaur::GetAll()) {
		e->Update(dt);
	}

	Mantis::SelfCollide();
	for (Mantis* e : Mantis::GetAll()) {
		e->Update(dt);
	}

	for (FlyingAlien* e : FlyingAlien::GetAll()) {
		e->Update(dt);
	}

	for (FireSlime* e : FireSlime::GetAll()) {
		e->Update(dt);
	}

	for (const BoxBounds& a : Tiled::Areas::trigger_lava) {
		if (a.Contains(player.pos)) {
			raising_lava->SetLevel(raising_lava_target_height);
		}
	}

	for (const BoxBounds& a : Tiled::Areas::trigger_fast_lava) {
		if (a.Contains(player.pos)) {
			raising_lava->SetRaiseSpeed(Lava::kFastRaiseSpeed);
		}
	}

#ifdef _DEBUG
	const SDL_Scancode killall = SDL_SCANCODE_F11;
	const SDL_Scancode unlockbasics = SDL_SCANCODE_F8;
	const SDL_Scancode teleport = SDL_SCANCODE_F9;
	const SDL_Scancode screen_left = SDL_SCANCODE_F6;
	const SDL_Scancode screen_right = SDL_SCANCODE_F7;
	const SDL_Scancode restart = SDL_SCANCODE_F5;
	if (Keyboard::IsKeyJustPressed(restart)) {
		// actual restart is done in main.cpp, this is here only to clear the save
		if (Keyboard::IsKeyPressed(SDL_SCANCODE_LSHIFT)) {
			SaveState saveState = SaveState::Open(kSaveStateGameName, saveSlot);
			saveState.Clear();
			saveState.Save();
		}
	}
	if (Keyboard::IsKeyJustPressed(teleport)) {
		player.pos = Tiled::Entities::single_debug_teleport;
		screenManager.UpdateCurrentScreen(player.pos);
		Camera::SetCenter(player.GetCameraTargetPos());
	}
	if (Keyboard::IsKeyJustPressed(unlockbasics)) {
		skillTree.Enable(Skill::ATTACK);
		skillTree.Enable(Skill::DIVE);
		skillTree.Enable(Skill::DASH);
		skillTree.Enable(Skill::WALLJUMP);
		skillTree.Enable(Skill::BREAK);
		if (Keyboard::IsKeyPressed(SDL_SCANCODE_LSHIFT)) {
			skillTree.Enable(Skill::GUN);
		}
	}
	if (Keyboard::IsKeyJustPressed(screen_left)) {
		player.pos.x -= Window::GAME_WIDTH;
	}
	if (Keyboard::IsKeyJustPressed(screen_right)) {
		player.pos.x += Window::GAME_WIDTH;
	}
	if (Keyboard::IsKeyJustPressed(killall)) {
		for (Bat* e : Bat::GetAll()) {
			if (e->screen == screenManager.CurrentScreen()) {
				DieWithSmallExplosion(e);
			}
		}
		for (FireSlime* e : FireSlime::GetAll()) {
			if (e->screen == screenManager.CurrentScreen()) {
				DieWithSmallExplosion(e);
			}
		}
	}

	if (Debug::Draw && Keyboard::IsKeyPressed(SDL_SCANCODE_LSHIFT)) {
		map.DebugEdit();
	}
#endif

	for (EnemyDoor* ed : EnemyDoor::GetAll()) {
		// If clossed, it checks for enemies with alive == false to open
		ed->Update(dt);
	}

	for (SaveStation* ss : SaveStation::GetAll()) {
		// If hidden, it checks for enemies with alive == false to unhide
		if (ss->Update(dt)) { // true if player can interact with it
			contextActionButton = true;
			if (Input::IsJustPressed(0, GameKeys::ACTION)) {
				// TODO: Interaction animation
				SaveGame();
				// Exit and Enter the scene again, resetting the state of everything and loading the state we just saved
				Fx::ScreenTransition::Start(Assets::fadeOutDiamondsShader);
			}
		}
	}

	if (boss_bipedal && !boss_bipedal->alive) {
		boss_bipedal = nullptr;
	}

	// Delete enemies after updating doors and savestations that might check for them
	Bat::DeleteNotAlive();
	Minotaur::DeleteNotAlive();
	Goomba::DeleteNotAlive();
	FlyingAlien::DeleteNotAlive();
	Mantis::DeleteNotAlive();
	FireSlime::DeleteNotAlive();
	Bipedal::DeleteNotAlive();

	for (HealthUp* g : HealthUp::GetAll()) {
		g->Update(dt);
	}

	for (Health* g : Health::GetAll()) {
		g->Update(dt);
	}
	Health::DeleteNotAlive();

	for (BigItem* g : BigItem::GetAll()) {
		if (Collide(g->Bounds(), player.Bounds())) {
			skillTree.Enable(g->skill);
			
			//TODO: Pickup animation or popup or something
			switch(g->skill) {
				case Skill::WALLJUMP:
					rotoText.ShowMessage("You remembered\nhow to walljump");
					break;
				case Skill::ATTACK:
					rotoText.ShowMessage("You remembered\nhow to fight!");
					break;
				case Skill::DIVE:
					rotoText.ShowMessage("You remembered\nhow to stab down");
					break;
				case Skill::DASH:
					rotoText.ShowMessage("You remembered\nhow to dash");
					break;
				case Skill::GUN:
					rotoText.ShowMessage("You remembered\nyour Big F. Gun");
					break;
				case Skill::BREAK:
					rotoText.ShowMessage("Your sword can\nnow break stuff!");
					break;
				default:
					break;
			}

			TriggerPickupItem(g, false);
			
			g->alive = false;

			SaveGame(); // silently save
		}
	}

	BigItem::DeleteNotAlive();

	Bullet::particles.UpdateParticles(dt);
	Missile::particles.UpdateParticles(dt);
	Health::particles.UpdateParticles(dt);

	destroyedTiles.Update(dt);

//	if (raising_lava->CurrentLevel() <= raising_lava_target_height+1.f) {
//		raising_lava->SetLevel(Tiled::Entities::single_lava_initial_height.y);
//	}
	for (Lava* l : Lava::GetAll()) {
		l->Update(dt);
	}

	for (const BoxBounds& a : Tiled::Areas::fog) {
		if (!Collide(Camera::Bounds(),(a*2.f))) {
			continue;
		}
		for (int x = 50; x < a.width - 180; x += 50) {
			for (int y = 0; y < a.height; y += 50) {
				fogPartSys.pos = vec(a.left + x, a.top + y);
				fogPartSys.Spawn(dt);
			}
		}
		fogPartSys.UpdateParticles(dt);
	}

	rotoText.Update(dt);

}

void JumpScene::Draw()
{
	Fx::FullscreenShader::Activate(); // Does nothing if no shader is set

	Window::Clear(31, 36, 50);

	DrawAllInOrder(
		Parallax::GetAll(),
		&map,
		&destroyedTiles,
		SaveStation::GetAll(),
		EnemyDoor::GetAll(),
		OneShotAnim::GetAll(),
		Goomba::GetAll(),
		Minotaur::GetAll(),
		Mantis::GetAll(),
		FlyingAlien::GetAll(),
		FireSlime::GetAll(),
		Bat::GetAll(),
		Bipedal::GetAll(),
		RocketLauncher::GetAll(),
		&Bullet::particles,
		&Missile::particles,
		Health::GetAll(),
		&Health::particles,
		Bullet::GetAll(),
		FireShot::GetAll(),
		Missile::GetAll(),
		HealthUp::GetAll(),
		BigItem::GetAll(),
		&player,
		Lava::GetAll()
	);

	if (contextActionButton) {
		Window::Draw(Assets::spritesheetTexture, player.Bounds().TopRight() + vec(2, -6))
			.withRect(Animation::GetRectAtTime(AnimLib::BUTTON_B_PRESS, mainClock));
	}

	rotoText.Draw(vec(0,-30));

#ifdef _IMGUI
	{
		ImGui::Begin("scene");
		vec m = Mouse::GetPositionInWorld();
		veci t = Tile::ToTiles(m);
		ImGui::Text("mainclock: %f", mainClock);
		ImGui::Text("Mouse: %f,%f", m.x, m.y);
		ImGui::Text("Mouse tile: %d,%d", t.x, t.y);

		if (ImGui::Button("Start waves")) {
			Fx::FullscreenShader::SetShader([]() {
				Assets::waveShader.Activate();
				Assets::waveShader.SetUniform("camera", Camera::Center()*Window::GetViewportScale()); 
				Assets::waveShader.SetUniform("time", mainClock * 10);
			});

		}
		if (ImGui::Button("Stop waves")) {
			Fx::FullscreenShader::SetShader(nullptr);
		}

		if (ImGui::Button("Save")) {
			SaveGame();
		}
		if (ImGui::Button("Load in same scene")) {
			LoadGame();
		}
		if (ImGui::Button("Load in new scene")) {
			LoadGame();
			SceneManager::RestartScene();
		}
		if (ImGui::Button("Clear save")) {
			SaveState::Open(kSaveStateGameName, saveSlot)
				.Clear()
				.Save();
		}
		ImGui::End();
	}
#endif

	//Health::particles.DrawImGUI("health");
	//Bullet::particles.DrawImGUI("BulletTrail");
	//Missile::particles.DrawImGUI("MissileSmoke");

	//for (const BoxBounds& a : Tiled::Areas::parallax_forest) {
		//Assets::fogShader.Activate();
		//Assets::fogShader.SetUniform("offset", vec(mainClock*0.2f, 0.f));
		//Assets::fogShader.SetUniform("time", mainClock);
		// The texture is not used by the shader at all
		//Window::Draw(Assets::spritesheetTexture, a);
		//Shader::Deactivate();
	//}

	//fogPartSys.Draw();
	//fogPartSys.DrawImGUI();

	//for (int i = 0; i < Parallax::GetAll().size(); i++) {
	//	Parallax::GetAll()[i]->DrawImGUI(("parallax_" + std::to_string(i)).c_str());
	//}

#ifdef _DEBUG
	if (Debug::Draw && Keyboard::IsKeyPressed(SDL_SCANCODE_LSHIFT)) {
		map.DebugEditDraw();
	}
#endif

	Fx::FullscreenShader::Deactivate(); // Does nothing if no shader was active

	Camera::InScreenCoords::Begin();
	player.DrawGUI();
	Camera::InScreenCoords::End();

	//player.polvito.DrawImGUI("Polvito");
}
