#include "KOTH1v1Plugin.h"
#include "bakkesmod\wrappers\includes.h"
#include "utils/parser.h"

BAKKESMOD_PLUGIN(KOTH1v1Plugin, "KOTH 1v1 Plugin", VERSION, PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING | PLUGINTYPE_SPECTATOR | PLUGINTYPE_BOTAI | PLUGINTYPE_REPLAY)

void KOTH1v1Plugin::debugLog(string debugString) {
	if (DEBUG) {
		cvarManager->log(debugString);
	}
}

int KOTH1v1Plugin::getTeamNumberByName(string teamName) {
	ServerWrapper game = gameWrapper->GetOnlineGame();
	ArrayWrapper<TeamWrapper> teams = game.GetTeams();
	for (int i = 0; i < teams.Count(); i++) {
		TeamWrapper team = teams.Get(i);

		// the player's team
		UnrealStringWrapper customTeamName = team.GetCustomTeamName();
		if (!customTeamName.IsNull() && customTeamName.ToString() == teamName) {
			return team.GetTeamNum();
		}
	}

	return -1;
}

void KOTH1v1Plugin::initializeTeamIDs() {
	debugLog("Initializing Team IDs");
	kingTeamID = getTeamNumberByName("King");
	notKingTeamID = getTeamNumberByName("Not King");
}

void KOTH1v1Plugin::hookIfKOTH(string eventName) {
	initializeTeamIDs();
	if (kingTeamID == -1 || notKingTeamID == -1) return;
	gameWrapper->HookEventPost("Function TAGame.Team_TA.EventScoreUpdated", bind(&KOTH1v1Plugin::OnScoreUpdated, this, placeholders::_1));
	gameWrapper->HookEventPost("Function TAGame.Car_TA.EventHitBall", bind(&KOTH1v1Plugin::OnHitBall, this, placeholders::_1));
	gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", bind(&KOTH1v1Plugin::unhookKOTHEvents, this, placeholders::_1));
	gameWrapper->HookEventPost("Function TAGame.GameEvent_Soccar_TA.Destroyed", bind(&KOTH1v1Plugin::unhookKOTHEvents, this, placeholders::_1));
	debugLog("KOTH events hooked");
}

void KOTH1v1Plugin::unhookKOTHEvents(string eventName) {
	gameWrapper->UnhookEvent("Function TAGame.Team_TA.EventScoreUpdated");
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.EventHitBall");
	gameWrapper->UnhookEvent("Function  Engine.PlayerController.EnterStartState");
	debugLog("KOTH Events unhooked");
}

void KOTH1v1Plugin::cachePlayerScores(bool clear) {	
	if (clear) {
		goalMap.clear();
	}

	ServerWrapper game = gameWrapper->GetOnlineGame();
	ArrayWrapper<PriWrapper> priList = game.GetPRIs();
	for (int i = 0; i < priList.Count(); i++) {
		PriWrapper pri = priList.Get(i);

		auto playerID = pri.GetUniqueId().ID;
		auto playerGoals = pri.GetMatchGoals();
		
		goalMap[playerID] = playerGoals;
	}
}

PriWrapper* KOTH1v1Plugin::findSelfPri(ServerWrapper game) {
	ArrayWrapper<PriWrapper> priList = game.GetPRIs();
	for (int i = 0; i < priList.Count(); i++) {
		PriWrapper pri = priList.Get(i);
		debugLog("Found Pri with steamID: " + to_string(pri.GetUniqueId().ID));
		if (pri.GetUniqueId().ID == gameWrapper->GetSteamID()) {
			debugLog("Found self by iterating");
			return &pri;
		}
	}
	return NULL;
}

PriWrapper* KOTH1v1Plugin::getLastGoalPri() {
	ServerWrapper game = gameWrapper->GetOnlineGame();
	ArrayWrapper<PriWrapper> priList = game.GetPRIs();

	for (int i = 0; i < priList.Count(); i++) {
		PriWrapper pri = priList.Get(i);

		auto playerID = pri.GetUniqueId().ID;
		auto playerGoals = pri.GetMatchGoals();

		map<unsigned long long, int>::iterator it = goalMap.find(playerID);
		int lastGoalCount = 0;
		if (it == goalMap.end()) {
			lastGoalCount = playerGoals;
		}
		else {
			lastGoalCount = it->second;
		}

		// if the player has scored
		if (playerGoals > lastGoalCount) {
			return &pri;
		}
	}

	return NULL;
}

void KOTH1v1Plugin::changeTeam(PriWrapper* self) {
	if (self->IsSpectator()) {
		debugLog("Spectator -> Not King");
		gameWrapper->HookEventPost(
			"Function Engine.PlayerController.EnterStartState",
			bind(&KOTH1v1Plugin::SpawnSpectator, this, placeholders::_1)
		);
		self->ServerChangeTeam(notKingTeamID);
		return;
	}

	PriWrapper* scoringPri = getLastGoalPri();
	auto selfID = self->GetUniqueId().ID;
	bool selfScoredGoal = scoringPri ?
						  scoringPri->GetUniqueId().ID == selfID : // Normal goal
						  lastTouchID != selfID; // No touch own goal

	if (selfScoredGoal && self->GetTeamNum() == kingTeamID) {
		debugLog("King -> King: No change");
		return;
	}
	if (selfScoredGoal) {
		debugLog("Not King -> King");
		self->ServerChangeTeam(kingTeamID);
		return;
	}

	debugLog("Switching to spectator");
	self->ServerSpectate();
}

void KOTH1v1Plugin::OnLoad() {
	gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Active.StartRound", bind(&KOTH1v1Plugin::hookIfKOTH, this, placeholders::_1));
	debugLog("Plugin loaded!");
}

void KOTH1v1Plugin::SpawnSpectator(string eventName) {
	debugLog("Spawning spectator");
	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		ServerWrapper game = gameWrapper->GetOnlineGame();
		ArrayWrapper<PriWrapper> priList = game.GetPRIs();
		for (int i = 0; i < priList.Count(); i++) {
			PriWrapper pri = priList.Get(i);
			debugLog("Found Pri with steamID: " + to_string(pri.GetUniqueId().ID));
			if (pri.GetUniqueId().ID == gameWrapper->GetSteamID()) {
				pri.ServerChangeTeam(notKingTeamID);
				pri.ServerChangeTeam(kingTeamID);
				pri.ServerChangeTeam(notKingTeamID);
				gameWrapper->UnhookEvent("Function  Engine.PlayerController.EnterStartState");
				return;
			}
		}
	}, 0.001f);
}

void KOTH1v1Plugin::OnUnload() {
	gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
	unhookKOTHEvents("Plugin Unloaded");
}

void KOTH1v1Plugin::OnScoreUpdated(string eventName) {
	if (!gameWrapper->IsInOnlineGame()) return;

	cachePlayerScores(true);

	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		OnLastGoal();
	}, 0.001f);
}

void KOTH1v1Plugin::OnHitBall(string eventName) {
	if (!gameWrapper->IsInOnlineGame()) return;

	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		ServerWrapper game = gameWrapper->GetOnlineGame();
		ArrayWrapper<PriWrapper> priList = game.GetPRIs();
		int mostRecentBallTouch = 0;

		for (int i = 0; i < priList.Count(); i++) {
			PriWrapper pri = priList.Get(i);

			if (pri.IsSpectator()) continue;

			CarWrapper car = pri.GetCar();
			if (car.IsNull()) continue;
				
			int priLastBallTouchFrame = car.GetLastBallTouchFrame();
			if (priLastBallTouchFrame > mostRecentBallTouch) {
				lastTouchID = pri.GetUniqueId().ID;
				mostRecentBallTouch = priLastBallTouchFrame;
			}
		}
	}, 0.001f);
}

void KOTH1v1Plugin::OnLastGoal() {
	ServerWrapper game = gameWrapper->GetOnlineGame();
	PriWrapper* self = NULL;

	CarWrapper localCar = gameWrapper->GetLocalCar();
	self = localCar.IsNull() ? findSelfPri(game) : &localCar.GetPRI();

	if (self == NULL) {
		debugLog("Unable to find self pri");
		return;
	}
	changeTeam(self);
}