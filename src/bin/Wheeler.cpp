#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "Serializer.h"
#include "Wheel.h"
#include "Wheeler.h"

#include "Input.h"
#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_internal.h"

#include "Controls.h"
#include "WheelEntry.h"
#include "WheelItems/WheelItem.h"
#include "WheelItems/WheelItemMutable.h"
#include "WheelItems/WheelItemSpell.h"
#include "WheelItems/WheelItemWeapon.h"
#include "include/lib/Drawer.h"

#include "Utils.h"

#include "Animation/TimeInterpolator/TimeInterpolatorManager.h"
void Wheeler::Update()
{
	float deltaTime = ImGui::GetIO().DeltaTime;
	TimeFloatInterpolatorManager::Update(deltaTime);
	TimeUintInterpolatorManager::Update(deltaTime);
	std::shared_lock<std::shared_mutex> lock(_wheelDataLock);
	using namespace Config::Styling::Wheel;
	if (!RE::PlayerCharacter::GetSingleton() || !RE::PlayerCharacter::GetSingleton()->Is3DLoaded()) {
		return;
	}
	// begin draw
	auto ui = RE::UI::GetSingleton();
	if (!ui) {
		return;
	}

	if (_state == WheelState::KClosed) {                  // should close
		if (ImGui::IsPopupOpen(_wheelWindowID)) {         // if it's open, close it
			ImGui::SetNextWindowPos(ImVec2(-100, -100));  // set the pop-up pos to be outside the screen space.
			ImGui::BeginPopup(_wheelWindowID);
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			//ImGui::GetIO().MouseDrawCursor = false;
			auto controlMap = RE::ControlMap::GetSingleton();
			if (controlMap) {
				controlMap->ignoreKeyboardMouse = false;
			}
			if (_editMode) {
				showEditModeVanillaMenus(ui);
			}
		}
		return;
	}
	// state is opened, opening, or closing, draw the wheel with different alphas.

	if (!ImGui::IsPopupOpen(_wheelWindowID)) {  // should open, but not opened yet
		//ImGui::GetIO().MouseDrawCursor = true;
		auto controlMap = RE::ControlMap::GetSingleton();
		if (controlMap) {
			controlMap->ignoreKeyboardMouse = true;
		}
		ImGui::OpenPopup(_wheelWindowID);
		_activeEntryIdx = -1;   // reset active item on reopen
		_cursorPos = { 0, 0 };  // reset cursor pos
	}

	ImGui::SetNextWindowPos(ImVec2(-100, -100));  // set the pop-up pos to be outside the screen space.

	if (shouldBeInEditMode(ui)) {
		if (!_editMode) {
			enterEditMode();
		}
		hideEditModeVanillaMenus(ui);
	} else {
		if (_editMode) {
			exitEditMode();
		}
	}

	bool poppedUp = _editMode ? ImGui::BeginPopupModal(_wheelWindowID) : ImGui::BeginPopup(_wheelWindowID);
	if (poppedUp) {
		RE::TESObjectREFR::InventoryItemMap inv = RE::PlayerCharacter::GetSingleton()->GetInventory();

		const ImVec2 wheelCenter = getWheelCenter();

		auto drawList = ImGui::GetWindowDrawList();

		drawList->PushClipRectFullScreen();

		bool atLeaseOneWheelPresent = !_wheels.empty();

		if (!atLeaseOneWheelPresent) {
			Drawer::draw_text(wheelCenter.x, wheelCenter.y, 0, 0, "No Wheel Present", 255, 255, 255, 255, 40.f);
		}

		auto& g = _wheels[_activeWheelIdx];
		std::unique_ptr<Wheel>& wheel = atLeaseOneWheelPresent ? _wheels[_activeWheelIdx] : nullptr;
		// draws the pie menu
		int numEntries = wheel ? wheel->entries.size() : -1;

		float cursorAngle = atan2f(_cursorPos.y, _cursorPos.x); // where the cursor is pointing to

		if (numEntries == 0) {
			Drawer::draw_text(wheelCenter.x, wheelCenter.y, 0, 0, "Empty Wheel", 255, 255, 255, 255, 40.f);
			// draw an arc so users don't get confused
			//drawList->PathArcTo(wheelCenter, (InnerCircleRadius + OuterCircleRadius) * 0.5f, 0.0f, IM_PI * 2.0f * 0.99f, 128);  // FIXME: 0.99f look like full arc with closed thick stroke has a bug now
			//drawList->PathStroke(BackGroundColor, true, OuterCircleRadius - InnerCircleRadius);
		}
		for (int entryIdx = 0; entryIdx < numEntries; entryIdx++) {
			// fancy math begin

			const float entryArcSpan = 2 * IM_PI / ImMax(ITEMS_MIN, numEntries);

			const float innerSpacing = InnerSpacing / InnerCircleRadius / 2;
			float entryInnerAngleMin = entryArcSpan * (entryIdx - 0.5f) + innerSpacing + IM_PI / 2;
			float entryInnerAngleMax = entryArcSpan * (entryIdx + 0.5f) - innerSpacing + IM_PI / 2;

			float entryOuterAngleMin = entryArcSpan * (entryIdx - 0.5f) + innerSpacing * (InnerCircleRadius / OuterCircleRadius) + IM_PI / 2;
			float entryOuterAngleMax = entryArcSpan * (entryIdx + 0.5f) - innerSpacing * (InnerCircleRadius / OuterCircleRadius) + IM_PI / 2;

			if (entryInnerAngleMax > IM_PI * 2) {
				entryInnerAngleMin -= IM_PI * 2;
				entryInnerAngleMax -= IM_PI * 2;
			}

			if (entryOuterAngleMax > IM_PI * 2) {
				entryOuterAngleMin -= IM_PI * 2;
				entryOuterAngleMax -= IM_PI * 2;
			}

			// update hovered item
			if (_cursorPos.x != 0 || _cursorPos.y != 0) {
				bool updatedActiveEntry = false;
				float cursorIndicatorToCenterDist = InnerCircleRadius - CursorIndicatorDist;
				Drawer::draw_arc(wheelCenter,
					cursorIndicatorToCenterDist - (CusorIndicatorArcWidth / 2),
					cursorIndicatorToCenterDist + (CusorIndicatorArcWidth / 2),
					cursorAngle - (CursorIndicatorArcAngle / 2), cursorAngle + (CursorIndicatorArcAngle / 2),
					cursorAngle - (CursorIndicatorArcAngle / 2), cursorAngle + (CursorIndicatorArcAngle / 2),
					CursorIndicatorColor,
					32);
				ImVec2 cursorIndicatorTriPts[3] = {
					{ cursorIndicatorToCenterDist + (CusorIndicatorArcWidth / 2), +CursorIndicatorTriangleSideLength },
					{ cursorIndicatorToCenterDist + (CusorIndicatorArcWidth / 2), -CursorIndicatorTriangleSideLength },
					{ cursorIndicatorToCenterDist + (CusorIndicatorArcWidth / 2) + CursorIndicatorTriangleSideLength, 0 }
				};
				for (ImVec2& pos : cursorIndicatorTriPts) {
					pos = ImRotate(pos, cos(cursorAngle), sin(cursorAngle));
				}
				Drawer::draw_triangle_filled(cursorIndicatorTriPts[0] + wheelCenter, cursorIndicatorTriPts[1] + wheelCenter, cursorIndicatorTriPts[2] + wheelCenter, CursorIndicatorColor);
				if (cursorAngle >= entryInnerAngleMin) {  // Normal case
					if (cursorAngle < entryInnerAngleMax) {
						if (entryIdx != _activeEntryIdx) {
							_activeEntryIdx = entryIdx;
							updatedActiveEntry = true;
						}
					}
				} else if (cursorAngle + 2 * IM_PI < entryInnerAngleMax && cursorAngle + 2 * IM_PI >= entryInnerAngleMin) {  // Wrap-around case
					if (entryIdx != _activeEntryIdx) {
						_activeEntryIdx = entryIdx;
						updatedActiveEntry = true;
					}
				}
				if (Config::Sound::EntrySwitchSound && updatedActiveEntry) {
					RE::PlaySoundRE(SD_ENTRYSWITCH);
				}
			}
			bool hovered = _activeEntryIdx == entryIdx;

			// calculate wheel center
			float t1 = (OuterCircleRadius - InnerCircleRadius) / 2;
			float t2 = InnerCircleRadius + t1;
			float rad = (entryInnerAngleMax - entryInnerAngleMin) / 2 + entryInnerAngleMin;
			ImVec2 itemCenter = ImVec2(
				wheelCenter.x + t2 * cosf(rad),
				wheelCenter.y + t2 * sinf(rad));

			int numArcSegments = (int)(256 * entryArcSpan / (2 * IM_PI)) + 1;

			WheelEntry* entry = wheel->entries[entryIdx];

			entry->Draw(wheelCenter, innerSpacing,
				entryInnerAngleMin, entryInnerAngleMax,
				entryOuterAngleMin, entryOuterAngleMax, itemCenter, hovered, numArcSegments, inv);
		}
		// draw cursor indicator
		//drawList->AddCircleFilled(_cursorPos + wheelCenter, 10, ImGui::GetColorU32(ImGuiCol_Border), 10);
		// draw wheel indicator
		for (int i = 0; i < _wheels.size(); i++) {
			if (i == _activeWheelIdx) {
				Drawer::draw_circle_filled(
					{ wheelCenter.x + Config::Styling::Wheel::WheelIndicatorOffsetX + i * Config::Styling::Wheel::WheelIndicatorSpacing,
						wheelCenter.y + Config::Styling::Wheel::WheelIndicatorOffsetY },
					Config::Styling::Wheel::WheelIndicatorSize, Config::Styling::Wheel::WheelIndicatorActiveColor, 10);
			} else {
				Drawer::draw_circle_filled(
					{ wheelCenter.x + Config::Styling::Wheel::WheelIndicatorOffsetX + i * Config::Styling::Wheel::WheelIndicatorSpacing,
						wheelCenter.y + Config::Styling::Wheel::WheelIndicatorOffsetY },
					Config::Styling::Wheel::WheelIndicatorSize, Config::Styling::Wheel::WheelIndicatorInactiveColor, 10);
			}
		}

doneDrawing:

		// update fade timer, alpha and wheel state.
		_openTimer += deltaTime;

		float alphaMult = 1.0f;
		switch (_state) {
		case WheelState::KOpening:
			if (_openTimer >= Config::Styling::Wheel::FadeTime) {
				_state = WheelState::KOpened;
			} else {
				alphaMult = _openTimer / Config::Styling::Wheel::FadeTime;
			}
			break;
		case WheelState::KClosing:
			_closeTimer += deltaTime;
			if (_closeTimer >= Config::Styling::Wheel::FadeTime) {
				CloseWheeler();
				_closeTimer = 0;
			} else {
				alphaMult = 1 - _closeTimer / Config::Styling::Wheel::FadeTime;
			}
			break;
		}

		Drawer::set_alpha_mult(alphaMult);
		drawList->PopClipRect();
		ImGui::EndPopup();
	}
}

void Wheeler::Clear()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (_state != WheelState::KClosed) {
		CloseWheeler();  // force close menu, since we're loading items
	}
	if (_editMode) {
		exitEditMode();
	}
	_activeEntryIdx = -1;
	// clean up old wheels
	for (auto& wheel : _wheels) {
		wheel->Clear();
	}
	_wheels.clear();
}

void Wheeler::ToggleWheeler()
{
	if (_state == WheelState::KClosed) {
		TryOpenWheeler();
	} else {
		TryCloseWheeler();
	}
}

void Wheeler::CloseWheelerIfOpenedLongEnough()
{
	if (_state == WheelState::KOpened && _openTimer > PRESS_THRESHOLD) {
		TryCloseWheeler();
	}
}

void Wheeler::TryOpenWheeler()
{
	// here we straight up open the wheel, and set state to opening if we have a fade time.
	// this is because for the fade to start showing the wheel has to be actually fully opened,
	// but to track the state we give it a "opening" state.
	OpenWheeler();
}

void Wheeler::TryCloseWheeler()
{
	if (_state == WheelState::KClosed || _state == WheelState::KClosing) {
		return;
	}
	if (Config::Styling::Wheel::FadeTime == 0) {
		CloseWheeler();  // close directly
	} else {
		// set timescale to 1 prior to closing the wheel to avoid weirdness
		if (Config::Styling::Wheel::SlowTimeScale <= 1) {
			if (Utils::Time::GGTM() != 1) {
				Utils::Time::SGTM(1);
			}
		}
		_state = WheelState::KClosing;  // set state to closing, will be closed once time out
		_closeTimer = 0;
	}
}

void Wheeler::OpenWheeler()
{
	if (!RE::PlayerCharacter::GetSingleton() || !RE::PlayerCharacter::GetSingleton()->Is3DLoaded()) {
		return;
	}
	if (_state != WheelState::KOpened && _state != WheelState::KOpening) {
		if (Config::Styling::Wheel::SlowTimeScale <= 1) {
			if (Utils::Time::GGTM() == 1) {
				Utils::Time::SGTM(Config::Styling::Wheel::SlowTimeScale);
			}
		}
		if (Config::Styling::Wheel::BlurOnOpen) {
			RE::UIBlurManager::GetSingleton()->IncrementBlurCount();
		}
		_state = Config::Styling::Wheel::FadeTime > 0 ? WheelState::KOpening : WheelState::KOpened;
		_openTimer = 0;
		RE::PlaySoundRE(SD_WHEELERTOGGLE);
	}
}

void Wheeler::CloseWheeler()
{
	if (!RE::PlayerCharacter::GetSingleton() || !RE::PlayerCharacter::GetSingleton()->Is3DLoaded()) {
		return;
	}
	if (_state != WheelState::KClosed) {
		if (Config::Styling::Wheel::SlowTimeScale <= 1) {
			if (Utils::Time::GGTM() != 1) {
				Utils::Time::SGTM(1);
			}
		}
		if (Config::Styling::Wheel::BlurOnOpen) {
			RE::UIBlurManager::GetSingleton()->DecrementBlurCount();
		}
		_openTimer = 0;
		_closeTimer = 0;
	}
	_state = WheelState::KClosed;
}

void Wheeler::UpdateCursorPosMouse(float a_deltaX, float a_deltaY)
{
	ImVec2 newPos = _cursorPos + ImVec2{ a_deltaX, a_deltaY };
	// Calculate the distance from the wheel center to the new cursor position
	float distanceFromCenter = sqrt(newPos.x * newPos.x + newPos.y * newPos.y);

	// If the distance exceeds the cursor radius, adjust the cursor position
	if (distanceFromCenter > Config::Control::Wheel::CursorRadiusPerEntry) {
		// Calculate the normalized direction vector from the center to the new position
		ImVec2 direction = newPos / distanceFromCenter;

		// Set the cursor position at the edge of the cursor radius
		newPos = direction * Config::Control::Wheel::CursorRadiusPerEntry;
	}

	_cursorPos = newPos;
}

void Wheeler::UpdateCursorPosGamepad(float a_x, float a_y)
{
	ImVec2 newPos = { a_x, a_y };
	float distanceFromCenter = sqrt(newPos.x * newPos.x + newPos.y * newPos.y);
	// If the distance exceeds the cursor radius, adjust the cursor position
	if (distanceFromCenter > Config::Control::Wheel::CursorRadiusPerEntry) {
		// Calculate the normalized direction vector from the center to the new position
		ImVec2 direction = newPos / distanceFromCenter;

		// Set the cursor position at the edge of the cursor radius
		newPos = direction * Config::Control::Wheel::CursorRadiusPerEntry;
	}

	_cursorPos = newPos;
}

void Wheeler::NextWheel()
{
	if (_state == WheelState::KOpened) {
		_cursorPos = { 0, 0 };
		_activeEntryIdx = -1;
		_activeWheelIdx += 1;
		if (_activeWheelIdx >= _wheels.size()) {
			_activeWheelIdx = 0;
		}
		if (Config::Sound::WheelSwitchSound && _wheels.size() > 1) {
			RE::PlaySoundRE(SD_WHEELSWITCH);
		}
	}
}

void Wheeler::PrevWheel()
{
	if (_state == WheelState::KOpened) {
		_cursorPos = { 0, 0 };
		_activeEntryIdx = -1;
		_activeWheelIdx -= 1;
		if (_activeWheelIdx < 0) {
			_activeWheelIdx = _wheels.size() - 1;
		}
		if (Config::Sound::WheelSwitchSound && _wheels.size() > 1) {
			RE::PlaySoundRE(SD_WHEELSWITCH);
		}
	}
}

void Wheeler::PrevItemInEntry()
{
	if (_state == WheelState::KOpened && _activeEntryIdx != -1) {
		_wheels[_activeWheelIdx]->PrevItemInEntry(_activeEntryIdx);
	}
}

void Wheeler::NextItemInEntry()
{
	if (_state == WheelState::KOpened && _activeEntryIdx != -1) {
		_wheels[_activeWheelIdx]->NextItemInEntry(_activeEntryIdx);
	}
}

void Wheeler::ActivateEntrySecondary()
{
	if (_state == WheelState::KOpened) {
		std::unique_ptr<Wheel>& activeWheel = _wheels[_activeWheelIdx];
		if (activeWheel->Empty()) {         // empty wheel, we can only delete in edit mode.
			if (_editMode && _wheels.size() > 1) {  // we have more than one wheel, so it's safe to delete this one.
				DeleteCurrentWheel();
			}
		} else if (_activeEntryIdx != -1) {
			if (activeWheel->ActivateEntrySecondary(_activeEntryIdx, _editMode)) { // an entry has been deleted, reset activeentryid, which on the next update will be reset to the correct entry.
				_activeEntryIdx = -1;
			}
		}
	}
}

void Wheeler::ActivateEntryPrimary()
{
	if (_state == WheelState::KOpened && _activeEntryIdx != -1 && !_wheels.empty()) {
		_wheels[_activeWheelIdx]->ActivateEntryPrimary(_activeEntryIdx, _editMode);
	}
}

void Wheeler::AddEmptyEntryToCurrentWheel()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (!_editMode || _wheels.empty() || _activeWheelIdx == -1) {
		return;
	}
	_wheels[_activeWheelIdx]->PushEmptyEntry();
}


void Wheeler::AddWheel()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (!_editMode) {
		return;
	}
	_wheels.push_back(std::make_unique<Wheel>());
}

void Wheeler::DeleteCurrentWheel()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (!_editMode) {
		return;
	}
	if (_wheels.size() > 1) {
		std::unique_ptr<Wheel>& toDelete = _wheels[_activeWheelIdx];
		if (!toDelete->Empty()) { // do not delete an non-empty wheel
			return;
		}
		_wheels.erase(_wheels.begin() + _activeWheelIdx);
		if (_activeWheelIdx == _wheels.size() && _activeWheelIdx != 0) {  //deleted the last wheel
			_activeWheelIdx = _wheels.size() - 1;
		}
		_activeEntryIdx = -1;  // no entry is active
	}
}

void Wheeler::MoveEntryForwardInCurrentWheel()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (!_editMode) {
		return;
	}
	if (_activeEntryIdx != -1 && _activeWheelIdx != -1) {
		_wheels[_activeWheelIdx]->MoveEntryForward(_activeEntryIdx);
	}
}

void Wheeler::MoveEntryBackInCurrentWheel()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (!_editMode) {
		return;
	}
	if (_activeEntryIdx != -1 && _activeWheelIdx != -1) {
		_wheels[_activeWheelIdx]->MoveEntryBack(_activeEntryIdx);
	}
}

void Wheeler::MoveWheelForward()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (!_editMode) {
		return;
	}
	if (_wheels.size() > 1) {
		if (_activeWheelIdx == _wheels.size() - 1) {
			// Move the current wheel to the very front
			auto currentWheel = std::move(_wheels.back());
			_wheels.pop_back();
			_wheels.insert(_wheels.begin(), std::move(currentWheel));
		} else {
			int targetIdx = _activeWheelIdx + 1;
			std::swap(_wheels[_activeWheelIdx], _wheels[targetIdx]);
			_activeWheelIdx = targetIdx;
		}
	}
}

void Wheeler::MoveWheelBack()
{
	std::unique_lock<std::shared_mutex> lock(_wheelDataLock);
	if (!_editMode) {
		return;
	}
	if (_wheels.size() > 1) {
		if (_activeWheelIdx == 0) {
			// Move the current wheel to the very end
			auto currentWheel = std::move(_wheels.front());
			_wheels.erase(_wheels.begin());
			_wheels.push_back(std::move(currentWheel));
			_activeWheelIdx = _wheels.size() - 1;
		} else {
			int targetIdx = _activeWheelIdx - 1;
			std::swap(_wheels[_activeWheelIdx], _wheels[targetIdx]);
			_activeWheelIdx = targetIdx;
		}
	}
}

int Wheeler::GetActiveWheelIndex()
{
	return _activeWheelIdx;
}

void Wheeler::SetActiveWheelIndex(int a_index)
{
	_activeWheelIdx = a_index;
}

inline ImVec2 Wheeler::getWheelCenter()
{
	return ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
}

bool Wheeler::shouldBeInEditMode(RE::UI* a_ui)
{
	return a_ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || a_ui->IsMenuOpen(RE::MagicMenu::MENU_NAME);
}

void Wheeler::hideEditModeVanillaMenus(RE::UI* a_ui)
{
	if (a_ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
		RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::InventoryMenu>()->uiMovie.get();
		if (uiMovie) {
			uiMovie->SetVisible(false);
		}
	}
	if (a_ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
		RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::MagicMenu>()->uiMovie.get();
		if (uiMovie) {
			uiMovie->SetVisible(false);
		}
	}
}

void Wheeler::showEditModeVanillaMenus(RE::UI* a_ui)
{
	if (a_ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
		RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::InventoryMenu>()->uiMovie.get();
		if (uiMovie) {
			uiMovie->SetVisible(true);
		}
	}
	if (a_ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
		RE::GFxMovieView* uiMovie = a_ui->GetMenu<RE::MagicMenu>()->uiMovie.get();
		if (uiMovie) {
			uiMovie->SetVisible(true);
		}
	}
}

void Wheeler::enterEditMode()
{
	if (_editMode) {
		return;
	}
	_editMode = true;
}

void Wheeler::exitEditMode()
{
	if (!_editMode) {
		return;
	}
	_editMode = false;
}
