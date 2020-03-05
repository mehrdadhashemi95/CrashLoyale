#include "Mob.h"

#include <memory>
#include <limits>
#include <stdlib.h>
#include <stdio.h>
#include "Building.h"
#include "Waypoint.h"
#include "GameState.h"
#include "Point.h"

int Mob::previousUUID;

Mob::Mob()
	: pos(-10000.f,-10000.f)
	, nextWaypoint(NULL)
	, targetPosition(new Point)
	, state(MobState::Moving)
	, uuid(Mob::previousUUID + 1)
	, attackingNorth(true)
	, health(-1)
	, targetLocked(false)
	, target(NULL)
	, lastAttackTime(0)
{
	Mob::previousUUID += 1;
}

void Mob::Init(const Point& pos, bool attackingNorth)
{
	health = GetMaxHealth();
	this->pos = pos;
	this->attackingNorth = attackingNorth;
	findClosestWaypoint();
}

std::shared_ptr<Point> Mob::getPosition() {
	return std::make_shared<Point>(this->pos);
}

bool Mob::findClosestWaypoint() {
	std::shared_ptr<Waypoint> closestWP = GameState::waypoints[0];
	float smallestDist = std::numeric_limits<float>::max();

	for (std::shared_ptr<Waypoint> wp : GameState::waypoints) {
		//std::shared_ptr<Waypoint> wp = GameState::waypoints[i];
		// Filter out any waypoints that are "behind" us (behind is relative to attack dir
		// Remember y=0 is in the top left
		if (attackingNorth && wp->pos.y > this->pos.y) {
			continue;
		}
		else if ((!attackingNorth) && wp->pos.y < this->pos.y) {
			continue;
		}

		float dist = this->pos.dist(wp->pos);
		if (dist < smallestDist) {
			smallestDist = dist;
			closestWP = wp;
		}
	}
	std::shared_ptr<Point> newTarget = std::shared_ptr<Point>(new Point);
	this->targetPosition->x = closestWP->pos.x;
	this->targetPosition->y = closestWP->pos.y;
	this->nextWaypoint = closestWP;

	return true;
}

void Mob::moveTowards(std::shared_ptr<Point> moveTarget, double elapsedTime) {
	Point movementVector;
	movementVector.x = moveTarget->x - this->pos.x;
	movementVector.y = moveTarget->y - this->pos.y;
	movementVector.normalize();
	movementVector *= (float)this->GetSpeed();
	movementVector *= (float)elapsedTime;
	pos += movementVector;
}


void Mob::findNewTarget() {
	// Find a new valid target to move towards and update this mob
	// to start pathing towards it

	if (!findAndSetAttackableMob()) { findClosestWaypoint(); }
}

// Have this mob start aiming towards the provided target
// TODO: impliment true pathfinding here
void Mob::updateMoveTarget(std::shared_ptr<Point> target) {
	this->targetPosition->x = target->x;
	this->targetPosition->y = target->y;
}

void Mob::updateMoveTarget(Point target) {
	this->targetPosition->x = target.x;
	this->targetPosition->y = target.y;
}


// Movement related
//////////////////////////////////
// Combat related

int Mob::attack(int dmg) {
	this->health -= dmg;
	return health;
}

bool Mob::findAndSetAttackableMob() {
	// Find an attackable target that's in the same quardrant as this Mob
	// If a target is found, this function returns true
	// If a target is found then this Mob is updated to start attacking it
	for (std::shared_ptr<Mob> otherMob : GameState::mobs) {
		if (otherMob->attackingNorth == this->attackingNorth) { continue; }

		bool imLeft    = this->pos.x     < (SCREEN_WIDTH / 2);
		bool otherLeft = otherMob->pos.x < (SCREEN_WIDTH / 2);

		bool imTop    = this->pos.y     < (SCREEN_HEIGHT / 2);
		bool otherTop = otherMob->pos.y < (SCREEN_HEIGHT / 2);
		if ((imLeft == otherLeft) && (imTop == otherTop)) {
			// If we're in the same quardrant as the otherMob
			// Mark it as the new target
			this->setAttackTarget(otherMob);
			return true;
		}
	}
	return false;
}

// TODO Move this somewhere better like a utility class
int randomNumber(int minValue, int maxValue) {
	// Returns a random number between [min, max). Min is inclusive, max is not.
	return (rand() % maxValue) + minValue;
}

void Mob::setAttackTarget(std::shared_ptr<Attackable> newTarget) {
	this->state = MobState::Attacking;
	target = newTarget;
}

bool Mob::targetInRange() {
	float range = this->GetSize(); // TODO: change this for ranged units
	float totalSize = range + target->GetSize();
	return this->pos.insideOf(*(target->getPosition()), totalSize);
}
// Combat related
////////////////////////////////////////////////////////////
// Collisions

// PROJECT 3:
//  1) return a vector of mobs that we're colliding with
//  2) handle collision with towers & river
std::vector<std::shared_ptr<Mob>> Mob::checkCollision() {
	// output changed to vector to return all collided mobs.
	std::vector<std::shared_ptr<Mob>> colided;
	for (std::shared_ptr<Mob> otherMob : GameState::mobs) {
		// don't collide with yourself
		if (this->sameMob(otherMob)) { continue; }

		// PROJECT 3: YOUR CODE CHECKING FOR A COLLISION GOES HERE
		float sizes = this->GetSize() + otherMob->GetSize();
		bool x_cond = sizes >= std::abs(this->getPosition()->x - otherMob->getPosition()->x + 0.5f);
		bool y_cond = sizes >= std::abs(this->getPosition()->y - otherMob->getPosition()->y + 0.5f);
		if (x_cond && y_cond) { colided.push_back(otherMob); }
	}
	return colided;
}

void Mob::processCollision(std::shared_ptr<Mob> otherMob, double elapsedTime) {
	// PROJECT 3: YOUR COLLISION HANDLING CODE GOES HERE
	// comparing their mass first
	if (this->GetMass() > otherMob->GetMass()) {
		return;
	} else {
		Point p = Point(this->pos.x - otherMob->getPosition()->x, this->pos.y - otherMob->getPosition()->y);
		p.normalize();
		// if |p.x| > |p.y|
		if (std::abs(p.x) > std::abs(p.y)) {
			if (p.x <= 0.0f && p.y <= 0.0f) {
				p.x += 0.5f;
				p.y -= 0.5f;
			} else if (p.x > 0.0f && p.y <= 0.0f) {
				p.x -= 0.5f;
				p.y -= 0.5f;
			} else if (p.x > 0.0f && p.y > 0.0f) {
				p.x -= 0.5f;
				p.y += 0.5f;
			} else if (p.x <= 0.0f && p.y > 0.0f) {
				p.x += 0.5f;
				p.y += 0.5f;
			}
		} else {
			if (p.x <= 0.0f && p.y <= 0.0f) {
				p.x -= 0.5f;
				p.y += 0.5f;
			} else if (p.x > 0.0f && p.y <= 0.0f) {
				p.x += 0.5f;
				p.y += 0.5f;
			} else if (p.x > 0.0f && p.y > 0.0f) {
				p.x += 0.5f;
				p.y -= 0.5f;
			} else if (p.x <= 0.0f && p.y > 0.0f) {
				p.x -= 0.5f;
				p.y -= 0.5f;
			}
		}
		p *= (float)this->GetSpeed();
		p *= (float)elapsedTime;
		// updating position
		pos += p;
	}
}

void Mob::checkBuildings(double elapsedTime) {
	for (std::shared_ptr<Building> building : GameState::buildings) {
		float sizeAvg = (building->GetSize() + this->GetSize()) / 2;
		bool x_cond = sizeAvg >= std::abs(this->getPosition()->x - building->getPosition()->x + 0.5f);
		bool y_cond = sizeAvg >= std::abs(this->getPosition()->y - building->getPosition()->y + 0.5f);
		if (x_cond && y_cond) {
			Point p = Point(this->pos.x - building->getPosition()->x, this->pos.y - building->getPosition()->y);
			p.normalize();
			if (p.x <= 0.0f && p.y <= 0.0f) {
				p.x -= 0.25;
				p.y += 0.25;
			} else if (p.x > 0.0f && p.y <= 0.0f) {
				p.x += 0.25;
				p.y += 0.25;
			} else if (p.x > 0.0f && p.y > 0.0f) {
				p.x += 0.25;
				p.y -= 0.25;
			} else if (p.x <= 0.0f && p.y > 0.0f) {
				p.x -= 0.25;
				p.y -= 0.25;
			}
			p *= (float)this->GetSpeed();
			p *= (float)elapsedTime;
			pos += p;
		}
	}
}

void Mob::checkRiver(double elapsedTime) {
	std::shared_ptr<Point> river;
	if (this->getPosition()->x >= RIVER_LEFT_X
			&& this->getPosition()->x <= RIVER_LEFT_X + (LEFT_BRIDGE_CENTER_X - BRIDGE_WIDTH / 2.0)
			&& this->getPosition()->y >= RIVER_TOP_Y
			&& this->getPosition()->y <= RIVER_TOP_Y + (RIVER_BOT_Y - RIVER_TOP_Y)) {
				river = std::make_shared<Point>(Point(RIVER_LEFT_X, RIVER_TOP_Y));
	} else if (this->getPosition()->x >= (RIGHT_BRIDGE_CENTER_X + BRIDGE_WIDTH/2.0 - 0.5)
			&& this->getPosition()->x <= (RIGHT_BRIDGE_CENTER_X + BRIDGE_WIDTH/2.0 - 0.5) + (SCREEN_WIDTH - RIGHT_BRIDGE_CENTER_X - BRIDGE_WIDTH/2.0)
			&& this->getPosition()->y >= RIVER_TOP_Y
			&& this->getPosition()->y <= RIVER_TOP_Y + (RIVER_BOT_Y - RIVER_TOP_Y)) {
				river = std::make_shared<Point>(Point((RIGHT_BRIDGE_CENTER_X + BRIDGE_WIDTH / 2.0 - 0.5), RIVER_TOP_Y));
	} else if (this->getPosition()->x >= (LEFT_BRIDGE_CENTER_X + BRIDGE_WIDTH/2.0 - 0.5)
			&& this->getPosition()->x <= (LEFT_BRIDGE_CENTER_X + BRIDGE_WIDTH/2.0 - 0.5) + (RIGHT_BRIDGE_CENTER_X - LEFT_BRIDGE_CENTER_X - BRIDGE_WIDTH)
			&& this->getPosition()->y >= RIVER_TOP_Y
			&& this->getPosition()->y <= RIVER_TOP_Y + (RIVER_BOT_Y - RIVER_TOP_Y)) {
				river = std::make_shared<Point>(Point((LEFT_BRIDGE_CENTER_X + BRIDGE_WIDTH/2.0 - 0.5), RIVER_TOP_Y));
	} else {
		// mob didn't hit the river.
		return;
	}
	Point p = Point(this->pos.x - targetPosition->x, this->pos.y - targetPosition->y);
	p.normalize();
	if (river->x == (BRIDGE_WIDTH + LEFT_BRIDGE_CENTER_X / 2.0) - 0.5) {
		if (this->pos.x > river->x + ((RIGHT_BRIDGE_CENTER_X - LEFT_BRIDGE_CENTER_X - BRIDGE_WIDTH) / 2)) {
			p.x += 0.5f;
		} else {
			p.x -= 0.5f;
		}
	} else if (river->x == RIVER_LEFT_X) { p.x += 0.5f; }
	else { p.x -= 0.5f; }
	p *= (float)this->GetSpeed();
	p *= (float)elapsedTime;
	pos += p;
}

// Collisions
///////////////////////////////////////////////
// Procedures

void Mob::attackProcedure(double elapsedTime) {
	if (this->target == nullptr || this->target->isDead()) {
		this->targetLocked = false;
		this->target = nullptr;
		this->state = MobState::Moving;
		return;
	}

	if (targetInRange()) {
		if (this->lastAttackTime >= this->GetAttackTime()) {
			// If our last attack was longer ago than our cooldown
			this->target->attack(this->GetDamage());
			this->lastAttackTime = 0; // lastAttackTime is incremented in the main update function
			return;
		}
	}
	else {
		// If the target is not in range
		moveTowards(target->getPosition(), elapsedTime);
		// handling collisions again
		std::vector<std::shared_ptr<Mob>> otherMobs = this->checkCollision();
		for (std::shared_ptr<Mob> m : otherMobs) {
			if (m) { this->processCollision(m, elapsedTime); }
		}
		// checking for building collisions
		this->checkBuildings(elapsedTime);
		// checking for river collisions
		this->checkRiver(elapsedTime);
	}
}

void Mob::moveProcedure(double elapsedTime) {
	if (targetPosition) {
		moveTowards(targetPosition, elapsedTime);

		// Check for collisions
		if (this->nextWaypoint->pos.insideOf(this->pos, (this->GetSize() + WAYPOINT_SIZE))) {
			std::shared_ptr<Waypoint> trueNextWP = this->attackingNorth ?
												   this->nextWaypoint->upNeighbor :
												   this->nextWaypoint->downNeighbor;
			setNewWaypoint(trueNextWP);
		}

		// PROJECT 3: You should not change this code very much, but this is where your
		// collision code will be called from
		// ********** NOTE: changes made to check collision against all mobs
		std::vector<std::shared_ptr<Mob>> otherMobs = this->checkCollision();
		for (std::shared_ptr<Mob> m : otherMobs) {
			if (m) { this->processCollision(m, elapsedTime); }
		}
		// checking for building collisions
		this->checkBuildings(elapsedTime);
		// checking for river collisions
		this->checkRiver(elapsedTime);

		// Fighting otherMob takes priority always
		findAndSetAttackableMob();
		// else
		findNewTarget();

	} else {
		// if targetPosition is nullptr
		findNewTarget();
	}
}

void Mob::update(double elapsedTime) {

	switch (this->state) {
	case MobState::Attacking:
		this->attackProcedure(elapsedTime);
		break;
	case MobState::Moving:
	default:
		this->moveProcedure(elapsedTime);
		break;
	}

	this->lastAttackTime += (float)elapsedTime;
}
