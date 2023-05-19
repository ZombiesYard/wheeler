#include "Controls.h"
#include "Wheeler.h"
void Controls::Init()
{
	for (const auto pair :
		std::vector<std::pair<KeyId, FunctionPtr>>
		{
			{ 0x12, &Wheeler::NextWheel }, // e
			{ 0x10, &Wheeler::PrevWheel }, // q
			{ 58, &Wheeler::ToggleMenu },        // caps lock
			{ 264, &Wheeler::PrevItem }, // mouse wheel up
			{ 265, &Wheeler::NextItem }, // mouse wheel down
			{ 256, &Wheeler::ActivateItemRight}, // mouse left
			{ 257, &Wheeler::ActivateItemLeft}, // mouse right
			{ 258, &Wheeler::TestAddItemToWheel }  // middle mouse
		}
		) {
		BindInput(pair.first, pair.second, true, false);
	}
	for (const auto pair :
		std::vector<std::pair<KeyId, FunctionPtr>>{
			{ 58, &Wheeler::CloseMenuIfOpenedLongEnough },  // caps lock
		}) {
		BindInput(pair.first, pair.second, false, false);
	}
}
void Controls::BindInput(KeyId key, FunctionPtr func, bool isDown, bool isGamePad)
{
	// do a linear search for the old key(can't have 2 keys bound to the same function)
	if (isDown) {
		if (isGamePad) {
			for (auto& [k, f] : _keyFunctionMapDownGamepad) {
				if (f == func) {
					_keyFunctionMapDownGamepad.erase(k);
					break;
				}
			}
			_keyFunctionMapDownGamepad[key] = func;
		} else {
			for (auto& [k, f] : _keyFunctionMapDown) {
				if (f == func) {
					_keyFunctionMapDown.erase(k);
					break;
				}
			}
			_keyFunctionMapDown[key] = func;
		}
		
	} else {
		if (isGamePad) {
			for (auto& [k, f] : _keyFunctionMapUpGamepad) {
				if (f == func) {
					_keyFunctionMapUpGamepad.erase(k);
					break;
				}
			}
			_keyFunctionMapUpGamepad[key] = func;
		} else {
			for (auto& [k, f] : _keyFunctionMapUp) {
				if (f == func) {
					_keyFunctionMapUp.erase(k);
					break;
				}
			}
			_keyFunctionMapUp[key] = func;
		}
		
	}
	
}

bool Controls::IsKeyBound(KeyId key)
{
	return _keyFunctionMapDown.contains(key) || _keyFunctionMapUp.contains(key) || _keyFunctionMapDownGamepad.contains(key) || _keyFunctionMapUpGamepad.contains(key);
}

void Controls::Dispatch(KeyId key, bool isDown, bool isGamePad)
{
	if (isDown) {
		if (isGamePad) {
			if (_keyFunctionMapDownGamepad.contains(key)) {
				FunctionPtr func = _keyFunctionMapDownGamepad[key];
				func();
			}
		} else {
			if (_keyFunctionMapDown.contains(key)) {
				FunctionPtr func = _keyFunctionMapDown[key];
				func();
			}
		}

	} else {
		if (isGamePad) {
			if (_keyFunctionMapUpGamepad.contains(key)) {
				FunctionPtr func = _keyFunctionMapUpGamepad[key];
				func();
			}
		} else {
			if (_keyFunctionMapUp.contains(key)) {
				FunctionPtr func = _keyFunctionMapUp[key];
				func();
			}
		}

	}
}
