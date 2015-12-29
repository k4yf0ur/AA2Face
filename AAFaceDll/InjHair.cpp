#include "InjHair.h"
#include "Logger.h"
#include "GenUtils.h"

INT_PTR g_HairDialogProcReturnValue = 0;
HWND g_edHairSelector = NULL;
HWND g_udHairSelector = NULL;

namespace {
	int loc_lastHairTab = -1;
	int loc_chosenHairs[4] = { -1,-1,-1,-1 };
	bool loc_hairButtonClicked = false;
	bool loc_bUdHairChanged = false;
	bool loc_bEdIgnoreChange = false;
	int loc_changedHairDirection = 0;
	HairDialogClass* loc_hairclass = NULL;
}

void __cdecl InitHairSelector(HWND parent,HINSTANCE hInst) {
	//{l:1821 t : 558 r : 1863 b : 578}
	//{l:1865 t : 531 r : 1907 b : 551}
	DWORD x = 420,y = 485,xw = 33,yw = 20;
	g_edHairSelector = CreateWindowExW(WS_EX_CLIENTEDGE,
		L"EDIT",L"0",WS_CHILD | WS_VISIBLE | ES_NUMBER,
		x,y,xw,yw,parent,0,hInst,0);
	if (g_edHairSelector == NULL) {
		int error = GetLastError();
		g_Logger << Logger::Priority::ERR << "Could not create hair edit window: error code " << error << "\r\n";
	}
	else {
		g_Logger << Logger::Priority::INFO << "Successfully created hair edit with handle " << g_edHairSelector << "\r\n";
	}
	InitCCs(ICC_UPDOWN_CLASS);
	g_udHairSelector = CreateWindowExW(0,
		UPDOWN_CLASSW,NULL,WS_VISIBLE | WS_CHILD | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_SETBUDDYINT,
		0,0,0,0,parent,0,hInst,0);
	SendMessageW(g_udHairSelector,UDM_SETBUDDY,(WPARAM)g_edHairSelector,0);
	SendMessageW(g_udHairSelector,UDM_SETRANGE,0,MAKELPARAM(255,0));
	ShowWindow(g_udHairSelector,SW_HIDE);
}

//similar to GetFaceSelectorIndex (the face one)
//where tab is 0 (front), 1 (side), 2 (back) or 3 (the other one)
//and guiChosen is the value polled from the original gui function
int __cdecl GetHairSelectorIndex(int tab,int guiChosen) {
	if (tab < 0 || tab > 3) return -1; //shouldnt happen, but lets just go safe here
	if (loc_hairButtonClicked) {
		//apply gui choice to our edit box, cause apparently the user clicked on a button,
		//so he probably wants that to show up
		loc_hairButtonClicked = false;		
		loc_chosenHairs[tab] = guiChosen;
		loc_bEdIgnoreChange = true;
		SetEditNumber(g_edHairSelector,loc_chosenHairs[tab]);
		loc_changedHairDirection = 0; //shouldnt happen anyway
		return -1;
	}
	else if (tab != loc_lastHairTab) {
		loc_lastHairTab = tab;
		//hair tab changed, so we should refresh our value with the hair of this tab
		loc_bEdIgnoreChange = true;
		SetEditNumber(g_edHairSelector,loc_chosenHairs[tab]);
		loc_changedHairDirection = 0; //also shouldnt happen
		return loc_chosenHairs[tab];
	}
	else {
		//lets just always return our edit number for now and keep it in sync with the other numbersWM_SET
		int ret = GetEditNumber(g_edHairSelector);
		loc_changedHairDirection = ret - loc_chosenHairs[tab]; //negative if newchoice < oldchoice
		loc_chosenHairs[tab] = ret;
		if (ret < 0 || ret > 255) ret = -1;
		
		return ret;
	}
	return -1;
}

//this function is called twice, once before and once after execution of the init function.
//before == true at first and false at second call.
void __cdecl InitHairTab(HairDialogClass* internclass,bool before) {
	if (before) {
		RefreshHairSelectorPosition(internclass);
		//before the call, we take note of the hair slots used
		for (int i = 0; i < 4; i++) {
			BYTE* hairPtr = internclass->HairOfTab(i);
			if (hairPtr != NULL) {
				loc_chosenHairs[i] = *hairPtr;
			}
		}
	}
	else {
		//after the call, we look at the hair, and correct them if they were changed
		for (int i = 0; i < 4; i++) {
			BYTE* hairPtr = internclass->HairOfTab(i);
			if (hairPtr != NULL && loc_chosenHairs[i] != -1 && *hairPtr != loc_chosenHairs[i]) {
				*hairPtr = loc_chosenHairs[i];
			}
		}
		loc_lastHairTab = -1; //make sure that the edit knows that this value is new and has to be put into the edit field
	}
}

void __cdecl HairDialogAfterInit(HairDialogClass* internclass) {
	const HWND mainWnd = *g_AA2MainWndHandle;
	const HWND ourWnd = g_AA2DialogHandles[9];
	loc_hairclass = internclass;
}

//if this returns 0, the dialog proc will be executed normally.
//if this returns !=0, INT_PTR g_HairDialogProcReturnValue will be returned by the dialog proc
int __cdecl HairDialogNotification(HairDialogClass* internclass,HWND hwndDlg,UINT msg,WPARAM wparam,LPARAM lparam) {
	const DWORD idBt0 = 0x28D8; //looked like this, hope they are always like this
	const DWORD idBt134 = 0x29A0;
	//as we can see, those would make for 200 buttons.
	//theres a jump from 55 to 56 with 290F to 294A and thats probably not the only one.
	//so while this is rather interisting information, lets not rely on this.
	//wow, i dont even know where this part was used. it seems kinda out of place now.
	//anyway, i should probably change this to a switch at some point. its become really big
	if (msg == WM_INITDIALOG) {
		//lets do something convenient here
		float* zoom = GetMaxZoom();
		if (zoom != NULL) *zoom = 0.01f; //normal max would be 0.1
	}
	else if (msg == WM_DESTROY) {
		loc_hairclass = NULL;
	}
	else if (msg == HAIRMESSAGE_FLIPHAIR) {
		HWND cbFlip = internclass->GetFlipButtonWnd();
		SendMessageW(cbFlip,BM_SETCHECK,SendMessageW(cbFlip,BM_GETCHECK,0,0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED,0);
		internclass->SetHairChangeFlags(loc_lastHairTab);
	}
	else if (msg == HAIRMESSAGE_ADDHAIR) {
		if (loc_lastHairTab >= 0 && loc_lastHairTab <= 3) {
			int diff = (int)wparam;
			SetEditNumber(g_edHairSelector,loc_chosenHairs[loc_lastHairTab] + diff);
		}
	}
	else if (msg == HAIRMESSAGE_SETHAIR) {
		if (loc_lastHairTab >= 0 && loc_lastHairTab <= 3) {
			int slot = (int)wparam;
			SetEditNumber(g_edHairSelector,slot);
		}
	}
	else if (msg == HAIRMESSAGE_HAIRSIZEADD) {
		if (loc_lastHairTab >= 0 && loc_lastHairTab <= 3) {
			int diff = (int)wparam;
			HWND edHairSize = internclass->GetHairSizeEditWnd();
			int size = GetEditNumber(edHairSize) + diff;
			SetEditNumber(g_edHairSelector,size);
		}
	}
	else if (msg == HAIRMESSAGE_HAIRSIZESET) {
		if (loc_lastHairTab >= 0 && loc_lastHairTab <= 3) {
			int size = (int)wparam;
			HWND edHairSize = internclass->GetHairSizeEditWnd();
			SetEditNumber(edHairSize,size);
		}
	}
	else if (msg == WM_COMMAND && lparam != 0) {
		DWORD ctrlId = LOWORD(wparam);
		DWORD notification = HIWORD(wparam);
		HWND wnd = (HWND)lparam;
		if (notification == BN_CLICKED) {
			//make sure the clicked button is not a checkbox, but an actual pushbutton
			LONG styles = GetWindowLong(wnd,GWL_STYLE);
			if (!(styles & BS_CHECKBOX)) {
				loc_hairButtonClicked = true; //This method worked so fine with the face, lets just do that again
			}
		}
		else if (wnd == g_edHairSelector && loc_lastHairTab >= 0 && loc_lastHairTab <= 3) {
			if (notification == EN_UPDATE) {
				if(loc_bEdIgnoreChange) {
					//allows us to ignore one change as will, usually when we update the edit to fit a certain value
					loc_bEdIgnoreChange = false;
				}
				else if (loc_bUdHairChanged) {
					//i hate common controlls. set value back to original value
					loc_bUdHairChanged = false;
					loc_bEdIgnoreChange = true;
					SetEditNumber(g_edHairSelector,loc_chosenHairs[loc_lastHairTab]);
				}
				else {
					//edit has been changed, so draw the selected new face
					int ret = GetEditNumber(g_edHairSelector);
					bool changed = false;
					if (ret < 0) {
						//must not be < 0
						ret = 0;
						changed = true;
					}
					else if (ret > 255) {
						ret = 255;
						changed = true;
					}
					if (changed) {
						loc_bEdIgnoreChange = true;
						SetEditNumber(g_edHairSelector,ret);
					}
					internclass->SetHairChangeFlags(loc_lastHairTab);
					if (ret > 134) {
						//always enable flip switch for these hairs
						HWND wnd = internclass->GetFlipButtonWnd();
						EnableWindow(wnd,TRUE);
					}
				}
			}
		}
	}
	else if (msg == WM_NOTIFY) {
		//this part doesnt work; i cant seem to find the HairSlotExists function ._.
		/*NMHDR* param = (NMHDR*)lparam;
		if(param->code == UDN_DELTAPOS) {
		LPNMUPDOWN lpnmud = (LPNMUPDOWN)lparam;
		int oldPos = lpnmud->iPos;
		int wantNewPos = oldPos + lpnmud->iDelta;
		if(internclass->HairSlotExists(wantNewPos, loc_lastHairTab)) {
		g_HairDialogProcReturnValue = 0; //allow change
		return TRUE;
		}
		else {
		//so, in the given direction, we have to search for the first available hair
		const BYTE* slotField = internclass->GetHairSlotExistsField(loc_lastHairTab);
		if(slotField != NULL) {
		int direction = lpnmud->iDelta < 0 ? -1 : 1;
		int border = direction == -1 ? 0 : 255;
		int newPos = border;
		for (int i = wantNewPos; i != border; i += direction) {
		if(slotField[i] == 1) {
		//found
		newPos = i;
		break;
		}
		}
		//set edit field to actual new position
		wchar_t num[64];
		_itow_s(newPos,num,64,10);
		SendMessageW(g_edHairSelector,WM_SETTEXT,0,(LPARAM)num); //ED_CHANGED will do the rest
		}
		//color me surprised, but the fucking controll doesnt fucking work again.
		//god, i hate common controlls.
		//were gonna do a really cheap trick now, so that after this, the next edit change will be ignored.
		//not the first time we used a trick like that now, is it?
		loc_udHairChanged = true;
		g_HairDialogProcReturnValue = 1; //prevent change (not that he accepts that decision)
		return TRUE;
		}
		return TRUE;
		}*/
	}
	return FALSE;
}

/*
* Ok, since windows 10 anon had problems with that, lets try not to hardcode the position this time.
*/
void RefreshHairSelectorPosition(HairDialogClass* internclass) {
	//i never found the createwindow calls for the buttons. only a single createwindow call ever enters,
	//and the class of this is an identifier, presumably of a custom resource. i do not know how to edit
	//compiled resource files, but this would explain why windows 10 anon has a different layout.
	//this would however also mean that at least the 8 buttons per row would be constant, so we can start with that.
	DWORD nButtons = internclass->GetButtonCount();
	//DWORD x = 420,y = 485,xw = 33,yw = 20; these were our default coordinates
	DWORD x = 420,y = 485;
	DWORD xw = 33,yw = 20;
	if (nButtons > 8) {
		//this should really be the case. i mean, usually its 135.
		HWND bt8 = internclass->GetHairSlotButton(7); //remember, slots are 0 based
		RECT rct = GetRelativeRect(bt8);
		//take xw and yw from this one
		xw = rct.right - rct.left;
		yw = rct.bottom - rct.top;
		//we take the x position from this one, as its the far right button
		x = rct.left;
	}
	//and the y of nButtons-1
	if (nButtons > 1) {
		HWND btLast = internclass->GetHairSlotButton(nButtons - 1);
		RECT rct = GetRelativeRect(btLast);
		//refresh size, just in case
		xw = rct.right - rct.left;
		yw = rct.bottom - rct.top;
		//and the y from this one
		y = rct.top;
	}
	MoveWindow(g_edHairSelector,x,y,xw,yw,TRUE);
	SendMessageW(g_udHairSelector,UDM_SETBUDDY,(WPARAM)g_edHairSelector,0);
	g_Logger << Logger::Priority::INFO << "Moved Hair edit to position (" << x << "|" << y << "), size " << xw << "x" << yw << "\r\n";
}

void __cdecl InvalidHairNotifier() {
	if (loc_lastHairTab < 0 || loc_lastHairTab > 3) return;
	//so, this method is horribly inefficient and sucks hard, but this is how im gonna do it, cause i cant find a
	//better way.
	//we face 2 problems here:
	//sending a message doesnt work, because the flags are reset AFTER this call, so setting them with a send message
	//wont have any effect. further more, a WM_SETTEXT can hardly be posted as the buffer has to stay valid.
	//luckily, we have an integrated message that doesnt need a buffer and can be posted more easily.
	//next problem is that we need to know the direction to go. we have to save that at the get selector call and use here.
	if(loc_changedHairDirection == 0) {
		//ignore
	}
	else if(loc_changedHairDirection < 0) {
		//stop at 0
		if (loc_chosenHairs[loc_lastHairTab] > 0) {
			PostMessageW(g_AA2DialogHandles[9],HAIRMESSAGE_ADDHAIR,(WPARAM)-1,0);
		}
	}
	else if(loc_changedHairDirection > 0) {
		//stop at 255
		if (loc_chosenHairs[loc_lastHairTab] < 255) {
			PostMessageW(g_AA2DialogHandles[9],HAIRMESSAGE_ADDHAIR,(WPARAM)1,0);
		}
	}
}