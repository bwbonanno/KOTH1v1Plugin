#include "KOTH1v1Plugin.h"
#include "bakkesmod\wrappers\includes.h"
#include "utils/parser.h"

BAKKESMOD_PLUGIN(KOTH1v1Plugin, "KOTH 1v1 Plugin", VERSION, PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING | PLUGINTYPE_SPECTATOR | PLUGINTYPE_BOTAI | PLUGINTYPE_REPLAY)

void KOTH1v1Plugin::onLoad() {
	gameWrapper->HookEventPost("Function TAGame.Team_TA.EventScoreUpdated", bind(&KOTH1v1Plugin::OnScoreUpdated, this, placeholders::_1));
	gameWrapper->HookEventPost("Function TAGame.Car_TA.EventHitBall", bind(&KOTH1v1Plugin::OnHitBall, this, placeholders::_1));
	gameWrapper->HookEventPost("Function Engine.PlayerController.EnterStartState", bind(&KOTH1v1Plugin::OnSpawn, this, placeholders::_1));

	cvarManager->log("Plugin loaded!");
}

void KOTH1v1Plugin::OnSpawn(string eventName) {
	if (!fixing) {
		fixing = true;
		gameWrapper->SetTimeout([this](GameWrapper* gw) {
			ServerWrapper game = gameWrapper->GetOnlineGame();
			ArrayWrapper<PriWrapper> priList = game.GetPRIs();
			for (int i = 0; i < priList.Count(); i++) {
				PriWrapper pri = priList.Get(i);
				cvarManager->log("Found Pri with steamID: " + to_string(pri.GetUniqueId().ID));
				if (pri.GetUniqueId().ID == gameWrapper->GetSteamID()) {
					cvarManager->log("fixing spectator bug");

					pri.ServerChangeTeam(getTeamNumberByName("NOT KING"));
					pri.ServerChangeTeam(getTeamNumberByName("KING"));
					pri.ServerChangeTeam(getTeamNumberByName("NOT KING"));
					break;
				}
			}
		}, 0.001f);
	}
}

void KOTH1v1Plugin::onUnload() {
	gameWrapper->UnhookEvent("Function TAGame.Team_TA.EventScoreUpdated");
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.EventHitBall");
	gameWrapper->UnhookEvent("Function  Engine.PlayerController.EnterStartState");
	
}

void KOTH1v1Plugin::OnScoreUpdated(string eventName) {
	if (!gameWrapper->IsInOnlineGame()) return;

	cachePlayerScores(true);

	gameWrapper->SetTimeout([this](GameWrapper* gw) {
		actionLastGoal();
	}, 0.001f);
}

string KOTH1v1Plugin::getTeamNameByNumber(int teamNumber) {
	ServerWrapper game = gameWrapper->GetOnlineGame();
	ArrayWrapper<TeamWrapper> teams = game.GetTeams();
	for (int i = 0; i < teams.Count(); i++) {
		TeamWrapper team = teams.Get(i);

		// the player's team
		if (team.GetTeamNum() == teamNumber) {
			UnrealStringWrapper customTeamName = team.GetCustomTeamName();
			if (!customTeamName.IsNull()) {
				return customTeamName.ToString();
			}
		}
	}

	return "";
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

		if (lastTouchID == 0LL) return;

		//string lastTouchPlayerName = getPlayerNameByID(lastTouchID);
		//cvarManager->log(lastTouchPlayerName + " hit the ball");
	}, 0.001f);
}

string KOTH1v1Plugin::getPlayerNameByID(unsigned long long userID) {
	ServerWrapper game = gameWrapper->GetOnlineGame();
	ArrayWrapper<PriWrapper> priList = game.GetPRIs();
	for (int i = 0; i < priList.Count(); i++) {
		PriWrapper pri = priList.Get(i);

		if (pri.IsSpectator()) continue;

		if (pri.GetUniqueId().ID == userID) {
			CarWrapper car = pri.GetCar();
			if (car.IsNull()) continue;

			return car.GetOwnerName();
		}
	}

	return "";
}


void KOTH1v1Plugin::actionLastGoal() {
	ServerWrapper game = gameWrapper->GetOnlineGame();
	PriWrapper* selfPtr = NULL;

	CarWrapper localCar = gameWrapper->GetLocalCar();	
	if (localCar.IsNull()) {
		ArrayWrapper<PriWrapper> priList = game.GetPRIs();
		for (int i = 0; i < priList.Count(); i++) {
			PriWrapper pri = priList.Get(i);
			cvarManager->log("Found Pri with steamID: " + to_string(pri.GetUniqueId().ID));
			if (pri.GetUniqueId().ID == gameWrapper->GetSteamID()) {
				cvarManager->log("Found self by iterating");
				selfPtr = &pri;
				break;
			}
		}
	}
	else {
		selfPtr = &localCar.GetPRI();
	}
	
	if (selfPtr == NULL) {
		cvarManager->log("selfPtr is still NULL!");
		return;
	}

	PriWrapper self = *selfPtr;
	string selfTeam = getTeamNameByNumber(self.GetTeamNum());
	auto selfID = self.GetUniqueId().ID;
	bool selfScoredGoal = false;

	PriWrapper* scoringPri = getLastGoalPri();
	// no-touch own-goal -- check who scored it
	if (scoringPri == NULL) {
		// only count as a point if self DIDN'T have the last touch, aka a no-touch own-goal
		selfScoredGoal = (lastTouchID != selfID);
		//cvarManager->log("No-touch own-goal scored by " + getPlayerNameByID(lastTouchID));
	} else {
		selfScoredGoal = (scoringPri->GetUniqueId().ID == selfID);
		/*
		CarWrapper scoringCar = scoringPri->GetCar();
		if (!scoringCar.IsNull()) {
			cvarManager->log("Goal scored by " + scoringCar.GetOwnerName() + " | team: " + getTeamNameByNumber(scoringPri->GetTeamNum()));
		}
		*/
	}


	// spectator + goal = not king
	// spectator + no goal = not king

	// not king + goal = king
	// king + goal = king
	// not king + no goal = spectator
	// king + no goal = spectator

	if (self.IsSpectator()) {
		cvarManager->log("Spectator -> Not King");
		cvarManager->log("GetSpectatorShortcut: " + to_string(self.GetSpectatorShortcut()));

		fixing = false;
		self.ServerChangeTeam(getTeamNumberByName("NOT KING"));
		return;
	}

	if (selfScoredGoal) {
		if (selfTeam == "NOT KING") {
			cvarManager->log("Not King -> King");
			self.ServerChangeTeam(getTeamNumberByName("KING"));
		} else {
			cvarManager->log("King, no change");
		}
	} else {
		cvarManager->log(selfTeam + " -> Spectator");
		self.ServerSpectate();
	}
}