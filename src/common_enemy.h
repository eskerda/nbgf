#pragma once

#include "jumpman.h"
#include "bullet.h"
#include "collide.h"
#include "health.h"
#include "screen.h"
#include "rand.h"
#include "oneshotanim.h"
#include "anim_lib.h"
#include "bounds.h"
#include "assets.h"

inline bool InSameScreenAsPlayer(int myScreen) {
	return myScreen == -1 || myScreen == ScreenManager::instance()->CurrentScreen();
}

template<typename B>
Bullet* ReceiveDamageFromBullets(const B& bounds) { // returns true if collided
	for (Bullet* b : Bullet::GetAll()) {
		if (!b->alive) continue;
		if (Collide(b->Bounds(), bounds)) {
			b->explode();
			return b;
		}
	}
	return nullptr;
}

template<typename B>
bool DamagePlayerOnCollision(const B& bounds) { // returns true if collided
	JumpMan* player = JumpMan::instance();
	if (Collide(player->Bounds(), bounds)) {
		if (!player->isInvencible()) {
			player->TakeDamage(bounds.Center());
		}
		return true;
	}
	return false;
}

inline void RandomlySpawnHealth(vec pos, int percentChance = 10) {
	if (Rand::PercentChance(percentChance)) {
		new Health(pos + Rand::VecInRange(-6, -6, 6, 6));
	}
}

inline void DieWithSmallExplosion(Entity* e) {
	e->alive = false;
	new OneShotAnim(Assets::spritesheetTexture, e->pos, AnimLib::MAGIC_EXPLOSION, 1.3f);
	RandomlySpawnHealth(e->pos);
}

template<typename BoundsIterable>
inline int FindIndexOfSmallestBoundsContaining(vec pos, const BoundsIterable& bounds_array) {
	int smallest_i = -1;
	float smallest_area = Mates::MaxFloat;
	int i = 0;
	for (const auto& bounds : bounds_array) {
		if (bounds.Contains(pos)) {
			float area = bounds.Area();
			if (area < smallest_area) {
				smallest_i = i;
				smallest_area = area;
			}
		}
		i++;
	}
	return smallest_i;
}
