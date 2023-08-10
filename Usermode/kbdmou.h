#pragma once
#include <cstdint>
#include "KbdMouDef.h"
#include "Comm.h"


class KbdMou
{
private:
	void* device;

public:
	KbdMou();
	~KbdMou();

private:
	bool send_mouse_input(PMOUSE_INPUT_DATA input_data);
	bool send_keyboard_input(PKEYBOARD_INPUT_DATA input_data);

public:
	bool mouse_move(int x, int y, int flag);
	bool left_button_down();
	bool left_button_up();
	bool left_button_press(int delay);
	bool right_button_down();
	bool right_button_up();
	bool right_button_press(int delay);

	bool key_down(int vk_code);
	bool key_up(int vk_code);
	bool key_press(int vk_code, int delay);

};

