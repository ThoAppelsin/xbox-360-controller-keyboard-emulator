#include <Windows.h>
#include <Xinput.h>
#include <stdio.h>
#include <WinUser.h>
#pragma comment(lib, "XInput.lib") 

#define LabelLength			15
#define ButtonCount			24
#define CustomThreshold		1250
#define defaultInterDelay	30
#define rapidInterDelay		1
#define releaseallthekeys	for ( int k = 0; k < 24; k++ ) inp[k].ki.dwFlags = KEYEVENTF_KEYUP;	\
							SendInput( inpcount, inp, sizeof * inp );							\
							for ( int k = 0; k < 24; k++ ) inp[k].ki.dwFlags = 0
#define gPad( LR, XY )		gamepadstate.Gamepad.sThumb ## LR ## XY
#define LMbit_16bits( a )	( ( a & 0x8000 ) == 0x8000 )
#define kPressed( keyID )	LMbit_16bits( GetAsyncKeyState( keyID ) ) 
#define CooldownHandler		for ( int k = 0; k < 256; k++ ) {				\
								if ( Cooldowns[k] > 0 ) Cooldowns[k]--;		\
								else if ( TempBannedKeys[k] ) {				\
									TempBannedKeys[k] = 0;					\
									Cooldowns[k] = 0;						\
								}											\
							}
#define gPtPad( LR, XY )	gamepadstate->Gamepad.sThumb ## LR ## XY
#define LeftStkThreshold	93210321
#define RightStkThreshold	53210321


unsigned int ConstStringPrinter( DWORD preDelay, const char printee[], DWORD interPrDelay ) {
	unsigned int i;
	if ( preDelay ) Sleep( preDelay );
	for ( i = 0; printee[i] != '\0'; i++ ) {
		if ( interPrDelay ) Sleep( interPrDelay + ( printee[i] == ' ' ) * interPrDelay * 3 / 2 );
		putchar( printee[i] );
		if ( interPrDelay ) Sleep( interPrDelay * ( printee[i] == '.' && ( printee[i + 1] == ' ' || printee[i + 1] == '\n' ) ) * 20 );
	}
	return i;
}

unsigned int StringPrinter( DWORD preDelay, char printee[], DWORD interPrDelay ) {
	unsigned int i;
	if ( preDelay ) Sleep( preDelay );
	for ( i = 0; printee[i] != '\0'; i++ ) {
		if ( interPrDelay ) Sleep( interPrDelay * ( 2 * ( printee[i] == ' ' ) + 1 ) );
		putchar( printee[i] );
	}
	return i;
}

unsigned int IntPrinter( DWORD preDelay, long long printee, DWORD interPrDelay ) {
	unsigned int i;
	char b[20];
	sprintf_s( b, 20, "%lld", printee );
	if ( preDelay ) Sleep( preDelay );
	for ( i = 0; b[i] != '\0'; i++ ) {
		if ( interPrDelay ) Sleep( interPrDelay );
		putchar( b[i] );
	}
	return i;
}

char WaitforKey( int a ){
	while ( 1 ) {
		if ( GetConsoleWindow( ) == GetForegroundWindow( ) ) {
			if ( kPressed( a ) ) return 1;
			if ( kPressed( VK_ESCAPE ) ) return 0;
		}
		Sleep( 25 );
	}
}

char IsHex( char * a ) {
	return ( *a >= '0' && *a <= '9' ) || ( *a >= 'A' && *a <= 'F' ) || ( *a >= 'a' && *a <= 'f' );
}

char StatementFinder( FILE * configurationfile, fpos_t * a, fpos_t * b, unsigned int * len ) {
	int currentchar;
	char open = 0;
	char closed = 0;
	if ( len ) *len = 0;
	if ( !configurationfile || !a || !b ) return 2;

	while ( ( currentchar = getc( configurationfile ) ) != EOF ) {

		if ( open < 2 ) {
			if ( currentchar == '>' ) {
				if ( open ) fgetpos( configurationfile, a );
				open++;
			}
			else open = 0;
		}

		else if ( closed < 2 ) {
			if ( len ) *len += 1;
			if ( currentchar == '<' ) {
				if ( closed ) {
					if ( len ) *len -= 2;
					fgetpos( configurationfile, b );
					return 1;
				}
				closed++;
			}
			else closed = 0;
		}

	}

	return 0;
}

char cfgfilecheckup(
	FILE * configurationfile,
	const char cfgfilewarning[],
	unsigned int warninglength,

	int * invalidstatement,
	int * invalidcontroller,
	int * invalidbuttonID,
	int * invalidkeyID,
	int * warningstatement,
	char * awarningisontop,
	char assignmentstatementfor[ButtonCount] )
{

	int candidate;
	char firststatement = 1;
	fpos_t positionA;
	fpos_t positionB;
	unsigned int len;

	if ( !configurationfile || !invalidstatement || !invalidcontroller || !invalidbuttonID || !invalidkeyID || !warningstatement || !awarningisontop ) return 2;

	while ( StatementFinder( configurationfile, &positionA, &positionB, &len ) ) {
		fsetpos( configurationfile, &positionA );
		if ( len == 7 ) {
			char cStatement[7];
			for ( char i = 0; i < 7; i++ ) cStatement[i] = getc( configurationfile );
			if ( cStatement[1] == ':' && cStatement[4] == ':' ) {

				if ( cStatement[0] != '0' ) *invalidcontroller += 1;

				candidate = 10 * ( cStatement[2] - '0' );
				candidate += cStatement[3] - '0';
				if ( candidate >= ButtonCount || candidate < 0 ) *invalidbuttonID += 1;
				else assignmentstatementfor[candidate] += 1;

				if ( !IsHex( cStatement + 5 ) || !IsHex( cStatement + 6 ) ) *invalidkeyID += 1;

			}
			else *invalidstatement += 1;
		}
		else if ( len == warninglength ) {
			char matches = 1;
			for ( int i = 0; cfgfilewarning[i] != '\0'; i++ ) if ( getc( configurationfile ) != cfgfilewarning[i] ) { matches = 0; break; }
			if ( matches ) {
				*warningstatement += 1;
				if ( firststatement ) *awarningisontop = 1;
			}
			else *invalidstatement += 1;
		}
		else *invalidstatement += 1;

		fsetpos( configurationfile, &positionB );
		firststatement = 0;
	}

	if ( *invalidstatement || *invalidcontroller || *invalidbuttonID || *invalidkeyID || ( *warningstatement != 1 ) || !(*awarningisontop) ) return 0;
	for ( int i = 1; i < ButtonCount; i++ ) if ( assignmentstatementfor[i] != 1 ) return 0;

	return 1;
}

void assigner( int j, WORD * translations, char * label, char * PermBannedKeys, char * TempBannedKeys, char * Cooldowns, FILE * configurationfile, XINPUT_STATE * gamepadstate ) {

	WORD keyID;
	DWORD previousPacketNumber = 0;
	unsigned int aligner;

	if ( !translations || !label || !PermBannedKeys || !TempBannedKeys || !Cooldowns || !gamepadstate || j > ButtonCount || j < 0 ) return;

	if ( XInputGetState( 0, gamepadstate ) == ERROR_SUCCESS ) previousPacketNumber = gamepadstate->dwPacketNumber;
	
	ConstStringPrinter( 75, "- ", rapidInterDelay );
	aligner = LabelLength - StringPrinter( 0, label, rapidInterDelay );
	putchar( ':' );
	while ( --aligner > 0 ) putchar( ' ' );

	
	while ( 1 ) {
		
		if ( GetConsoleWindow( ) == GetForegroundWindow( ) ) {
			keyID = 8;			/*
								Keys prior to 0x08 are
								01: Left mouse button
								02: Right mouse button
								03: Control-break processing
								04: Middle mouse button
								05: X1 mouse button
								06: X2 mouse button
								07: Undefined
								all of which I couldn't make work
								... including the ones under PermBannedKeys
								... including the ones beyond E5
								*/


			while ( keyID < 0xE5 ) {
				if ( !PermBannedKeys[keyID] & !TempBannedKeys[keyID] & kPressed( keyID ) ) {
					translations[j] = keyID;
					if ( configurationfile ) {
						int temp;
						rewind( configurationfile );
						while ( ( temp = getc( configurationfile ) ) != EOF ) {
							if ( temp == '>' ) if ( getc( configurationfile ) == '>' ) if ( getc( configurationfile ) == '0' ) {
								getc( configurationfile );
								if ( getc( configurationfile ) == j / 10 + '0' ) if ( getc( configurationfile ) == j % 10 + '0' ) {
									getc( configurationfile );
									fseek( configurationfile, 0L, SEEK_CUR );
									fprintf_s( configurationfile, "%02X", keyID );
									putchar( '*' );
									break;
								}
							}
						}

					}
					puts( "done" );
					Cooldowns[keyID] = 10;
					TempBannedKeys[keyID] = 1;
					return;
				}
				keyID++;
			}

			if ( XInputGetState( 0, gamepadstate ) == ERROR_SUCCESS ) if ( gamepadstate->dwPacketNumber != previousPacketNumber ) {
				if ( ( gamepadstate->Gamepad.wButtons & XINPUT_GAMEPAD_A ) == XINPUT_GAMEPAD_A ) {
					translations[j] = 0;
					if ( configurationfile ) {
						int temp;
						rewind( configurationfile );
						while ( ( temp = getc( configurationfile ) ) != EOF ) {
							if ( temp == '>' ) if ( getc( configurationfile ) == '>' ) if ( getc( configurationfile ) == '0' ) {
								getc( configurationfile );
								if ( getc( configurationfile ) == j / 10 + '0' ) if ( getc( configurationfile ) == j % 10 + '0' ) {
									getc( configurationfile );
									fseek( configurationfile, 0L, SEEK_CUR );
									fprintf_s( configurationfile, "%02X", 0 );
									putchar( '*' );
									break;
								}
							}
						}

					}
					ConstStringPrinter( 0, "discarded\n", defaultInterDelay );
					return;
				}
				if ( ( gamepadstate->Gamepad.wButtons & XINPUT_GAMEPAD_B ) == XINPUT_GAMEPAD_B ) {
					ConstStringPrinter( 0, "skipped\n", defaultInterDelay );
					return;
				}
			}
		}

		CooldownHandler
		Sleep( 20 );
	}

}

void solo_assigner(
	WORD * translations,
	char labels[ButtonCount][LabelLength],
	char * PermBannedKeys,
	char * TempBannedKeys,
	char * Cooldowns,
	FILE * configurationfile,
	XINPUT_STATE * gamepadstate,
	WORD buttonbit[ButtonCount] ) {

	DWORD previousPacketNumber = 0;

	puts( "1: Letterlike Buttons" );
	puts( "2: Bumpers & Triggers" );
	puts( "3: Directional Pad" );
	puts( "4: Miscellaneous; start/back, left/right stk press" );
	puts( "5: L Stick Directionals" );
	puts( "6: R Stick Directionals" );
	puts( "Gamepad Button itself" );
	puts( "Escape to exit\n" );

	if ( XInputGetState( 0, gamepadstate ) == ERROR_SUCCESS ) previousPacketNumber = gamepadstate->dwPacketNumber;

	while ( 1 ) {
		if ( GetConsoleWindow( ) == GetForegroundWindow( ) ) {
			for ( int i = 0; i < 6; i++ ) {
				if ( !TempBannedKeys[i + 0x31] & kPressed( i + 0x31 ) | !TempBannedKeys[i + 0x61] & kPressed( i + 0x61 ) ) {

					Cooldowns[i + 0x31] = 10;
					TempBannedKeys[i + 0x31] = 1;
					Cooldowns[i + 0x61] = 10;
					TempBannedKeys[i + 0x61] = 1;

					for ( int j = 0; j < 4; j++ ) {
						printf_s( "%d: %s\n", j + 1, labels[4 * i + j] );
					}
					puts( "Gamepad Button itself" );
					puts( "Backspace to select another set" );
					puts( "Escape to exit\n" );

					while ( 1 ) {
						if ( GetConsoleWindow( ) == GetForegroundWindow( ) ) {
							for ( int j = 0; j < 4; j++ ) {
								if ( !TempBannedKeys[j + 0x31] & kPressed( j + 0x31 ) | !TempBannedKeys[j + 0x61] & kPressed( j + 0x61 ) ) {

									Cooldowns[j + 0x31] = 10;
									TempBannedKeys[j + 0x31] = 1;
									Cooldowns[j + 0x61] = 10;
									TempBannedKeys[j + 0x61] = 1;

									assigner( 4 * i + j, translations, labels[4 * i + j], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
									putchar( 10 );
									solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
									return;

								}
							}

							if ( !TempBannedKeys[VK_ESCAPE] & kPressed( VK_ESCAPE ) ) {
								Cooldowns[VK_ESCAPE] = 10;
								TempBannedKeys[VK_ESCAPE] = 1;
								return;
							}

							if ( !TempBannedKeys[VK_BACK] & kPressed( VK_BACK ) ) {
								Cooldowns[VK_BACK] = 10;
								TempBannedKeys[VK_BACK] = 1;
								solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
								return;
							}

							if ( XInputGetState( 0, gamepadstate ) == ERROR_SUCCESS ) if ( gamepadstate->dwPacketNumber != previousPacketNumber ) {

								for ( int i = 0; i < 6; i++ )
									if ( ( gamepadstate->Gamepad.wButtons & buttonbit[i] ) == buttonbit[i] ) {
										assigner( i, translations, labels[i], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										putchar( 10 );
										solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
										return;
									}

								if ( gamepadstate->Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) {
									assigner( 6, translations, labels[6], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
									putchar( 10 );
									solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
									return;
								}

								if ( translations[7] && ( gamepadstate->Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ) {
									assigner( 7, translations, labels[7], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
									putchar( 10 );
									solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
									return;
								}

								for ( int i = 8; i < 16; i++ )
									if ( ( gamepadstate->Gamepad.wButtons & buttonbit[i] ) == buttonbit[i] ) {
										assigner( i, translations, labels[i], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										putchar( 10 );
										solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
										return;
									}

								if ( gPtPad( L, X ) * gPtPad( L, X ) + gPtPad( L, Y ) * gPtPad( L, Y ) > LeftStkThreshold * 4 ) {
									if ( gPtPad( L, Y ) * gPtPad( L, Y ) > gPtPad( L, X ) * gPtPad( L, X ) * 3 ) {
										if ( gPtPad( L, Y ) > 0 ) assigner( 16, translations, labels[16], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										else assigner( 17, translations, labels[17], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										putchar( 10 );
										solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
										return;
									}
									if ( gPtPad( L, X ) * gPtPad( L, X ) > gPtPad( L, Y ) * gPtPad( L, Y ) * 3 ) {
										if ( gPtPad( L, X ) > 0 ) assigner( 18, translations, labels[18], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										else assigner( 19, translations, labels[19], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										putchar( 10 );
										solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
										return;
									}
								}

								if ( gPtPad( R, X ) * gPtPad( R, X ) + gPtPad( R, Y ) * gPtPad( R, Y ) > RightStkThreshold * 4 ) {
									if ( gPtPad( R, Y ) * gPtPad( R, Y ) > gPtPad( R, X ) * gPtPad( R, X ) * 3 ) {
										if ( gPtPad( R, Y ) > 0 ) assigner( 20, translations, labels[20], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										else assigner( 21, translations, labels[21], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										putchar( 10 );
										solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
										return;
									}
									if ( gPtPad( R, X ) * gPtPad( R, X ) > gPtPad( R, Y ) * gPtPad( R, Y ) * 3 ) {
										if ( gPtPad( R, X ) > 0 ) assigner( 22, translations, labels[22], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										else assigner( 23, translations, labels[23], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
										putchar( 10 );
										solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
										return;
									}
								}
							}

						}

						CooldownHandler
						Sleep( 20 );
					}
				}
			}

			if ( XInputGetState( 0, gamepadstate ) == ERROR_SUCCESS ) if ( gamepadstate->dwPacketNumber != previousPacketNumber ) {
				
				for ( int i = 0; i < 6; i++ )
					if ( ( gamepadstate->Gamepad.wButtons & buttonbit[i] ) == buttonbit[i] ) {
						assigner( i, translations, labels[i], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						putchar( 10 );
						solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
						return;
					}
				
				if ( gamepadstate->Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) {
					assigner( 6, translations, labels[6], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
					putchar( 10 );
					solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
					return;
				}

				if ( translations[7] && ( gamepadstate->Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ) {
					assigner( 7, translations, labels[7], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
					putchar( 10 );
					solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
					return;
				}

				for ( int i = 8; i < 16; i++ )
					if ( ( gamepadstate->Gamepad.wButtons & buttonbit[i] ) == buttonbit[i] ) {
						assigner( i, translations, labels[i], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						putchar( 10 );
						solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
						return;
					}

				if ( gPtPad( L, X ) * gPtPad( L, X ) + gPtPad( L, Y ) * gPtPad( L, Y ) > LeftStkThreshold * 4 ) {
					if ( gPtPad( L, Y ) * gPtPad( L, Y ) > gPtPad( L, X ) * gPtPad( L, X ) * 3 ) {
						if ( gPtPad( L, Y ) > 0 ) assigner( 16, translations, labels[16], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						else assigner( 17, translations, labels[17], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						putchar( 10 );
						solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
						return;
					}
					if ( gPtPad( L, X ) * gPtPad( L, X ) > gPtPad( L, Y ) * gPtPad( L, Y ) * 3 ) {
						if ( gPtPad( L, X ) > 0 ) assigner( 18, translations, labels[18], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						else assigner( 19, translations, labels[19], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						putchar( 10 );
						solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
						return;
					}
				}

				if ( gPtPad( R, X ) * gPtPad( R, X ) + gPtPad( R, Y ) * gPtPad( R, Y ) > RightStkThreshold * 4 ) {
					if ( gPtPad( R, Y ) * gPtPad( R, Y ) > gPtPad( R, X ) * gPtPad( R, X ) * 3 ) {
						if ( gPtPad( R, Y ) > 0 ) assigner( 20, translations, labels[20], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						else assigner( 21, translations, labels[21], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						putchar( 10 );
						solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
						return;
					}
					if ( gPtPad( R, X ) * gPtPad( R, X ) > gPtPad( R, Y ) * gPtPad( R, Y ) * 3 ) {
						if ( gPtPad( R, X ) > 0 ) assigner( 22, translations, labels[22], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						else assigner( 23, translations, labels[23], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );
						putchar( 10 );
						solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate, buttonbit );
						return;
					}
				}
			}

			if ( !TempBannedKeys[VK_ESCAPE] & kPressed( VK_ESCAPE ) ) {
				Cooldowns[VK_ESCAPE] = 10;
				TempBannedKeys[VK_ESCAPE] = 1;
				return;
			}

		}

		CooldownHandler
		Sleep( 20 );
	}
}

void mass_assigner( WORD * translations, char labels[ButtonCount][LabelLength], char * PermBannedKeys, char * TempBannedKeys, char * Cooldowns, FILE * configurationfile, XINPUT_STATE * gamepadstate ) {

	ConstStringPrinter( 75, "Press a key to assign for Xbox 360 Controller...\n", rapidInterDelay );

	for ( int j = 0; j < ButtonCount; j++ ) 
		assigner( j, translations, labels[j], PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, gamepadstate );

}

int main( )
{
	char labels[ButtonCount][LabelLength] = {
		"A button",		// 0
		"B button",
		"X button",
		"Y button",
		"R bumper",		// 4
		"L bumper",
		"R trigger",
		"L trigger",

		"D up",			// 8
		"D down",
		"D left",
		"D right",
		"start button",	// 12
		"back button",
		"L stk press",
		"R stk press",

		"L stk up",		// 16
		"L stk down",
		"L stk right",
		"L stk left",

		"R stk up",		// 20
		"R stk down",
		"R stk right",
		"R stk left",
	};
	 
	XINPUT_STATE gamepadstate;
	INPUT inp[ButtonCount];
	INPUT release[ButtonCount];
	WORD translations[24];
	char PermBannedKeys[256] = { 0 };
	char TempBannedKeys[256] = { 0 };
	char Cooldowns[256] = { 0 };
	FILE * configurationfile;
	const char cfgfilename[] = "Xbox360Mapper.cfg";
	const char cfgfilewarning[] = "Do not make any edits on the statements written between these two signs, including this warning text. Do not use these signs. Absence of this warning text as the initial statement or an uncompatible statment between the signs will be considered as corruption. Corruptions may not be automatically recovered, and result in loss of the configuration data, unless recovered by the user.";
	const char titleworking[] = "XBox 360 Controller Mapper - moving on";
	const char titleidle[] = "XBox 360 Controller Mapper - life... sucks.";
	const char titlewelcome[] = "XBox 360 Controller Mapper - Welcome!!";
	char cfgfileavailable = 0;

	SetConsoleTitle( titlewelcome );

	PermBannedKeys[0x0A] = 1;	// Reserved
	PermBannedKeys[0x0B] = 1;	// Reserved
	PermBannedKeys[0x0C] = 1;	// CLEAR key
	PermBannedKeys[0x0E] = 1;	// Undefined
	PermBannedKeys[0x0F] = 1;	// Undefined
	PermBannedKeys[0x15] = 1;	// IME Kana/Hanguel/Hangul mode
	PermBannedKeys[0x16] = 1;	// Undefined
	PermBannedKeys[0x17] = 1;	// IME Junga mode
	PermBannedKeys[0x18] = 1;	// IME final mode
	PermBannedKeys[0x19] = 1;	// IME Hanja/Kanji mode
	PermBannedKeys[0x1A] = 1;	// Undefined
	PermBannedKeys[0x1C] = 1;	// IME convert
	PermBannedKeys[0x1D] = 1;	// IME nonconvert
	PermBannedKeys[0x1E] = 1;	// IME accept
	PermBannedKeys[0x1F] = 1;	// IME mode change request
	PermBannedKeys[0x29] = 1;	// SELECT key
	PermBannedKeys[0x2A] = 1;	// PRINT key (not Print Screen)
	PermBannedKeys[0x2B] = 1;	// EXECUTE key
	PermBannedKeys[0x2F] = 1;	// HELP key (not F1)
	PermBannedKeys[0x3A] = 1;	// Undefined
	PermBannedKeys[0x3B] = 1;	// Undefined
	PermBannedKeys[0x3C] = 1;	// ...
	PermBannedKeys[0x3D] = 1;	// ..
	PermBannedKeys[0x3E] = 1;	// ..
	PermBannedKeys[0x3F] = 1;	// ...
	PermBannedKeys[0x40] = 1;	// Undefined
	PermBannedKeys[0x5E] = 1;	// Reserved
	PermBannedKeys[0x5F] = 1;	// Computer Sleep key
	PermBannedKeys[0x6C] = 1;	// Seperator key
	PermBannedKeys[0x7C] = 1;	// F13
	PermBannedKeys[0x7D] = 1;	// F14
	PermBannedKeys[0x7E] = 1;	// F15
	PermBannedKeys[0x7F] = 1;	// ...
	PermBannedKeys[0x80] = 1;	// ..
	PermBannedKeys[0x81] = 1;	// ..
	PermBannedKeys[0x82] = 1;	// .
	PermBannedKeys[0x83] = 1;	// ..
	PermBannedKeys[0x84] = 1;	// ..
	PermBannedKeys[0x85] = 1;	// ...
	PermBannedKeys[0x86] = 1;	// F23
	PermBannedKeys[0x87] = 1;	// F24
	PermBannedKeys[0x88] = 1;	// Unassigned
	PermBannedKeys[0x89] = 1;	// Unassigned
	PermBannedKeys[0x8A] = 1;	// ...
	PermBannedKeys[0x8B] = 1;	// ..
	PermBannedKeys[0x8C] = 1;	// .
	PermBannedKeys[0x8D] = 1;	// ..
	PermBannedKeys[0x8E] = 1;	// ...
	PermBannedKeys[0x8F] = 1;	// Unassigned
	PermBannedKeys[0x92] = 1;	// OEM specific
	PermBannedKeys[0x93] = 1;	// OEM specific
	PermBannedKeys[0x94] = 1;	// OEM specific
	PermBannedKeys[0x95] = 1;	// OEM specific
	PermBannedKeys[0x96] = 1;	// OEM specific
	PermBannedKeys[0x97] = 1;	// Unassigned
	PermBannedKeys[0x98] = 1;	// Unassigned
	PermBannedKeys[0x99] = 1;	// ...
	PermBannedKeys[0x9A] = 1;	// ..
	PermBannedKeys[0x9B] = 1;	// .
	PermBannedKeys[0x9C] = 1;	// .
	PermBannedKeys[0x9D] = 1;	// ..
	PermBannedKeys[0x9E] = 1;	// ...
	PermBannedKeys[0x9F] = 1;	// Unassigned
	PermBannedKeys[0xB4] = 1;	// Start Mail key
	PermBannedKeys[0xB5] = 1;	// Select Media key
	PermBannedKeys[0xB6] = 1;	// Start Application 1 key
	PermBannedKeys[0xB7] = 1;	// Start Application 2 key
	PermBannedKeys[0xB8] = 1;	// Reserved
	PermBannedKeys[0xB9] = 1;	// Reserved
	PermBannedKeys[0xBB] = 1;	// For any country/region, the '+' key (not on mine)
	PermBannedKeys[0xC1] = 1;	// Reserved
	PermBannedKeys[0xC2] = 1;	// Reserved
	PermBannedKeys[0xC3] = 1;	// .....
	PermBannedKeys[0xC4] = 1;	// ....
	PermBannedKeys[0xC5] = 1;	// ...
	PermBannedKeys[0xC6] = 1;	// ...
	PermBannedKeys[0xC7] = 1;	// ...
	PermBannedKeys[0xC8] = 1;	// ..
	PermBannedKeys[0xC9] = 1;	// ..
	PermBannedKeys[0xCA] = 1;	// ..
	PermBannedKeys[0xCB] = 1;	// .
	PermBannedKeys[0xCC] = 1;	// .
	PermBannedKeys[0xCD] = 1;	// .
	PermBannedKeys[0xCE] = 1;	// .
	PermBannedKeys[0xCF] = 1;	// ..
	PermBannedKeys[0xD0] = 1;	// ..
	PermBannedKeys[0xD1] = 1;	// ..
	PermBannedKeys[0xD2] = 1;	// ...
	PermBannedKeys[0xD3] = 1;	// ...
	PermBannedKeys[0xD4] = 1;	// ...
	PermBannedKeys[0xD5] = 1;	// ....
	PermBannedKeys[0xD6] = 1;	// .....
	PermBannedKeys[0xD7] = 1;	// Reserved
	PermBannedKeys[0xD8] = 1;	// Unassigned
	PermBannedKeys[0xD9] = 1;	// Unassigned
	PermBannedKeys[0xDA] = 1;	// Unassigned
	PermBannedKeys[0xE0] = 1;	// Reserved
	PermBannedKeys[0xE1] = 1;	// OEM specific
	PermBannedKeys[0xE3] = 1;	// OEM specific

	if ( fopen_s( &configurationfile, cfgfilename, "r" ) ) {
		
		ConstStringPrinter( 500, "Configuration file (", defaultInterDelay );
		ConstStringPrinter( 0, cfgfilename, defaultInterDelay );
		ConstStringPrinter( 0, ") could not be found.\n", defaultInterDelay );

		if ( fopen_s( &configurationfile, cfgfilename, "w+" ) ) {

			ConstStringPrinter( 500, "A new file could not be created either...\n", defaultInterDelay );
			ConstStringPrinter( 500, "Programme can continue without one.\n", defaultInterDelay );
			
			ConstStringPrinter( 900, "\n========\n\n", 10 );
			
			ConstStringPrinter( 0, "Warning:", 40 );
			ConstStringPrinter( 400, " With no configuration file, your settings won't be saved.\n", 40 );

			ConstStringPrinter( 200, "\nInability to create a new configuration file is likely to be due to the directory of the executable. Moving the executable to somewhere else or opening the programme with administrative priviledges may resolve the issue.\n\nIn case you can, it is recommended for you to close the programme, resolve the issue, then retry.\n", defaultInterDelay );

			ConstStringPrinter( 900, "\n========\n\n", 10 );

			ConstStringPrinter( 0, "Hit Escape to exit.\nPress Enter to start the mass assignment process.\n", defaultInterDelay );

			if ( WaitforKey( VK_RETURN ) ) {
				Cooldowns[VK_RETURN] = 10;
				TempBannedKeys[VK_RETURN] = 1;
				mass_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, NULL, &gamepadstate );
			}
			else {
				ConstStringPrinter( 50, "Goodbye!!", 70 ); Sleep( 250 );
				return 0;
			}
		}
		else {
			fprintf_s( configurationfile, "Curious huh? Okay... This is the configuration file, automatically generated. Well, almost automatically, I have written this text myself. Anyway, I have one !!!WARNING!!! for you, here it goes:\n\n" );
			fprintf_s( configurationfile, ">>%s<<\n\n", cfgfilewarning );
			fprintf_s( configurationfile, "Here's the deal. Programme searches for the statement-introducing-signs (the double greater-than sign), reads whatever, until it reaches the outro. It really will look for the warning text, word by word, so do not touch it. Haha, just kidding!.. It actually looks for it character by character... Having a statement before the warning text is also not allowed, even if the statement is all correct. Text outside the intro and outro are disregarded, so... yeah. Ok, I truly am done with the warnings now.\n\n" );
			fprintf_s( configurationfile, "=== How to read ===\n- The first value, right after the intro sign, is the Gamepad Index (0~3). Tell me if you wish this programme supported more controllers, it only supports one right now.\n- The second value is the Controller Button Index (0~%d). Not necessarily just buttons, stick directionals are also there, whatever...\n- The last one is the Keyboard Key ID (0~255) for the buttons to be translated into. IDs are given in hexadecimal.\n\n=== Button Indexes ===\n", ButtonCount - 1 );
			for ( int i = 0; i < ButtonCount; i++ ) fprintf_s( configurationfile, "%02d: %s\t\t", i, labels[i] );
			fprintf_s( configurationfile, "\n\n=== Keyboard Key IDs ===\nHere's a list from Microsoft Developer Network: http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx\nNote: I have prevented some keys to be mapped during the assignment inside the programme. I recommend you not to try some random virtual key ID from there, but whatever... When ID is zero, the corresponding button won't get translated into anything.\n\n" );
			
			for ( int i = 0; i < ButtonCount; i++ ) fprintf_s( configurationfile, ">>0:%02d:00<<\n", i );

			cfgfileavailable = 1;

			ConstStringPrinter( 0, "Created a new file for future use.\n", defaultInterDelay );
			
			ConstStringPrinter( 200, "\n========\n\n", 10 );
			
			ConstStringPrinter( 0, "Press Enter to start the mass assignment process.", defaultInterDelay );
			ConstStringPrinter( 0, "\n", 0 );

			if ( WaitforKey( VK_RETURN ) ) {
				Cooldowns[VK_RETURN] = 10;
				TempBannedKeys[VK_RETURN] = 1;
				ConstStringPrinter( 0, "\n========\n\n", defaultInterDelay );
				mass_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, &gamepadstate );
			}
			else {
				ConstStringPrinter( 50, "Goodbye!!", 70 ); Sleep( 250 );
				return 0;
			}
		}
	}
	else {
		int invalidstatement = 0;
		int invalidcontroller = 0;
		int invalidbuttonID = 0;
		int invalidkeyID = 0;
		int warningstatement = 0;
		char awarningisontop = 0;
		char assignmentstatementfor[ButtonCount] = { 0 };

		if ( cfgfilecheckup( configurationfile, cfgfilewarning, sizeof cfgfilewarning / sizeof (*cfgfilewarning) - 1,
			&invalidstatement, &invalidcontroller, &invalidbuttonID, &invalidkeyID, &warningstatement, &awarningisontop, assignmentstatementfor ) )
		{
			fpos_t positionA;
			fpos_t positionB;
			char sStatement[7];
			int buttonID;
			WORD keyID;

			rewind( configurationfile );
			StatementFinder( configurationfile, &positionA, &positionB, NULL );

			while ( StatementFinder( configurationfile, &positionA, &positionB, NULL ) ) {
				fsetpos( configurationfile, &positionA );
				for ( char i = 0; i < 7; i++ ) sStatement[i] = getc( configurationfile );
				
				buttonID = 10 * ( sStatement[2] - '0' );
				buttonID += sStatement[3] - '0';

				if ( sStatement[5] >= 'a' ) keyID = 16 * ( 10 + sStatement[5] - 'a' );
				else if ( sStatement[5] >= 'A' ) keyID = 16 * ( 10 + sStatement[5] - 'A' );
				else keyID = 16 * ( sStatement[5] - '0' );

				if ( sStatement[6] >= 'a' ) keyID += 10 + sStatement[6] - 'a';
				else if ( sStatement[6] >= 'A' ) keyID += 10 + sStatement[6] - 'A';
				else keyID += sStatement[6] - '0';

				translations[buttonID] = keyID;
				fsetpos( configurationfile, &positionB );
			}

			ConstStringPrinter( 500, "Your previous settings have been successfully fetched.\n", defaultInterDelay );

			if ( freopen_s( &configurationfile, cfgfilename, "r+", configurationfile ) ){
				fopen_s( &configurationfile, cfgfilename, "r" );
				ConstStringPrinter( 0, "However, the file appears to be write-protected, meaning that you won't be able to change its contents through the programme configuration menu.\n", defaultInterDelay );
			}
			else {
				cfgfileavailable = 1;
			}

		}
		else {

			ConstStringPrinter( 500, "Corruption detected in the configuration file ", defaultInterDelay );
			ConstStringPrinter( 0, "(", 50 );
			ConstStringPrinter( 0, cfgfilename, 50 );
			ConstStringPrinter( 0, ")", 50 );
			ConstStringPrinter( 0, ".\n", defaultInterDelay );
			ConstStringPrinter( 0, "Bad news:", defaultInterDelay );

			ConstStringPrinter( 400, "\n\n========\n\n", 10 );

			if ( invalidstatement == 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidstatement, defaultInterDelay ); ConstStringPrinter( 0, " invalid statement", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }
			if ( invalidstatement > 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidstatement, defaultInterDelay ); ConstStringPrinter( 0, " invalid statements", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }

			if ( warningstatement == 0 ) { ConstStringPrinter( 0, "- Corrupted or missing warning statement", defaultInterDelay ); ConstStringPrinter( 200, ", I told you not to touch it...", defaultInterDelay * 2 ); ConstStringPrinter( 1000, "\n\n", 10 ); }
			else if ( warningstatement > 1 ) { ConstStringPrinter( 0, "- More than one warning statements", defaultInterDelay ); ConstStringPrinter( 250, ", what", 40 ); ConstStringPrinter( 250, " the", 40 ); ConstStringPrinter( 250, " fuck", 40 ); ConstStringPrinter( 150, "?", 40 ); ConstStringPrinter( 1000, "\n\n", 10 ); }
			else if ( !awarningisontop ) { ConstStringPrinter( 0, "- The warning statement is not the first statement...\n\n", defaultInterDelay ); ConstStringPrinter( 1200, "\n\n", 10 ); }

			if ( invalidcontroller == 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidcontroller, defaultInterDelay ); ConstStringPrinter( 0, " invalid controller index within settings", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }
			if ( invalidcontroller > 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidcontroller, defaultInterDelay ); ConstStringPrinter( 0, " invalid controller indexes within settings", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }

			if ( invalidbuttonID == 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidbuttonID, defaultInterDelay ); ConstStringPrinter( 0, " invalid controller button ID within settings", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }
			if ( invalidbuttonID > 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidbuttonID, defaultInterDelay ); ConstStringPrinter( 0, " invalid controller button IDs within settings", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }

			if ( invalidkeyID == 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidkeyID, defaultInterDelay ); ConstStringPrinter( 0, " invalid keyboard key ID within settings", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }
			if ( invalidkeyID > 1 ) { ConstStringPrinter( 0, "- ", defaultInterDelay ); IntPrinter( 0, invalidkeyID, defaultInterDelay ); ConstStringPrinter( 0, " invalid keyboard key IDs within settings", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }

			int counter = 0;

			for ( int i = 0; i < ButtonCount; i++ ) if ( assignmentstatementfor[i] > 1 ) counter++;
			if ( counter == 1 ) for ( int i = 0; i < ButtonCount; i++ ) if ( assignmentstatementfor[i] > 1 ) {
				 { ConstStringPrinter( 0, "- Multiple assigments for the button ", defaultInterDelay ); IntPrinter( 0, i, defaultInterDelay ); ConstStringPrinter( 0, " (", defaultInterDelay ); StringPrinter( 0, labels[i], defaultInterDelay ); ConstStringPrinter( 0, ")", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }
				break;
			}
			if ( counter > 1 ) {
				 { ConstStringPrinter( 0, "- Multiple assigments for the following buttons;", defaultInterDelay ); ConstStringPrinter( 250, "\n", 10 ); }
				for ( int i = 0; i < ButtonCount; i++ ) if ( assignmentstatementfor[i] > 1 ) { ConstStringPrinter( 0, "\n      ", 0 ); IntPrinter( 0, i, 10 ); ConstStringPrinter( 0, " (", 10 ); StringPrinter( 0, labels[i], 10 ); ConstStringPrinter( 0, ")", 10 ); }
				ConstStringPrinter( 1000, "\n\n", 10 );
			}

			counter = 0;

			for ( int i = 0; i < ButtonCount; i++ ) if ( assignmentstatementfor[i] == 0 ) counter++;
			if ( counter == 1 ) for ( int i = 0; i < ButtonCount; i++ ) if ( assignmentstatementfor[i] == 0 ) {
				{ ConstStringPrinter( 0, "- The button with the ID ", defaultInterDelay ); IntPrinter( 0, i, defaultInterDelay ); ConstStringPrinter( 0, " (", defaultInterDelay ); StringPrinter( 0, labels[i], defaultInterDelay ); ConstStringPrinter( 0, ") is not assigned", defaultInterDelay ); ConstStringPrinter( 1000, "\n\n", 10 ); }
				break;
			}
			if ( counter > 1 ) {
				{ ConstStringPrinter( 0, "- The buttons with the following IDs are not assigned;", defaultInterDelay ); ConstStringPrinter( 250, "\n", 10 ); }
				for ( int i = 0; i < ButtonCount; i++ ) if ( assignmentstatementfor[i] == 0 ) /*printf_s( "      %d (%s)\n", i, labels[i] );*/ { ConstStringPrinter( 0, "\n      ", 0 ); IntPrinter( 0, i, 10 ); ConstStringPrinter( 0, " (", 10 ); StringPrinter( 0, labels[i], 10 ); ConstStringPrinter( 0, ")", 10 ); }
				ConstStringPrinter( 1000, "\n\n", 10 );
			}

			ConstStringPrinter( 0, "========\n\n", 10 );

			ConstStringPrinter( 0, "Programme can continue without it.", defaultInterDelay );
			ConstStringPrinter( 900, "\n\nWarning:", 40 );
			ConstStringPrinter( 300, " With no configuration file, your settings won't be saved.", 40 );
			ConstStringPrinter( 500, "\nIt is recommended for you to;", 30 );
			ConstStringPrinter( 900, "\n  i.   close the programme now", defaultInterDelay );
			ConstStringPrinter( 900, ",\n  ii.  fix whatever", defaultInterDelay ); ConstStringPrinter( 500, " or rename/delete the ", defaultInterDelay ); ConstStringPrinter( 0, cfgfilename, defaultInterDelay );
			ConstStringPrinter( 1200, ",\n  iii. then retry.", defaultInterDelay );

			ConstStringPrinter( 800, "\n\n========\n\n", 10 );
			ConstStringPrinter( 0, "Hit Escape to exit.\nPress Enter to start the mass assignment process.", defaultInterDelay / 2 );
			ConstStringPrinter( 0, "\n", 0 );

			if ( WaitforKey( VK_RETURN ) ) {
				Cooldowns[VK_RETURN] = 10;
				TempBannedKeys[VK_RETURN] = 1;
				mass_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, NULL, &gamepadstate );
			}
			else { 
				ConstStringPrinter( 50, "Goodbye!!", 70 ); Sleep( 250 );
				return 0;
			}
		}
	}
	

	unsigned short Accumulation[2][4] = { 0 };	/*	0: LeftStick	0: Up    (+y)
													1: RightStick	1: Down  (-y)
																	2: Right (+x)
																	3: Left  (-x)	*/

	ZeroMemory( &gamepadstate, sizeof gamepadstate );
	ZeroMemory( inp, sizeof inp );
	ZeroMemory( release, sizeof release );

	for ( int i = 0; i < ButtonCount; i++ ) {
		inp[i].type = 1;
		release[i].type = 1;
		release[i].ki.dwFlags = KEYEVENTF_KEYUP;
	}
	
	DWORD previousPacketNumber = 0;
	int inpcount = 0;
	int releasecount = 0;
	char previouslyPressed[ButtonCount] = { 0 };
	WORD buttonbit[ButtonCount] = {
		XINPUT_GAMEPAD_A,
		XINPUT_GAMEPAD_B,
		XINPUT_GAMEPAD_X,
		XINPUT_GAMEPAD_Y,
		XINPUT_GAMEPAD_RIGHT_SHOULDER,
		XINPUT_GAMEPAD_LEFT_SHOULDER,
		0,
		0,
		XINPUT_GAMEPAD_DPAD_UP,
		XINPUT_GAMEPAD_DPAD_DOWN,
		XINPUT_GAMEPAD_DPAD_LEFT,
		XINPUT_GAMEPAD_DPAD_RIGHT,
		XINPUT_GAMEPAD_START,
		XINPUT_GAMEPAD_BACK,
		XINPUT_GAMEPAD_LEFT_THUMB,
		XINPUT_GAMEPAD_RIGHT_THUMB
	};
	int Connected = -1;
	int gamepadCheckCooldown = 0;

	puts( "\n========\n" );
	puts( "   Configuration menu:" );
	if ( cfgfileavailable ) puts( "\nThey will also be saved into your file.\n" );
	else puts( "\nNo file, so they will be temporary.\n" );
	puts( "1. Mass assignment (Gamepad A: discard, Gamepad B: skip)" );
	puts( "2. Solo assignment\n" );

	while ( 1 ) {

		if ( GetConsoleWindow( ) == GetForegroundWindow( ) ) {

			if ( !TempBannedKeys[0x31] & kPressed( 0x31 ) | !TempBannedKeys[0x61] & kPressed( 0x61 ) ) {
				releaseallthekeys;
				Cooldowns[0x31] = 10;
				TempBannedKeys[0x31] = 1;
				Cooldowns[0x61] = 10;
				TempBannedKeys[0x61] = 1;
				if ( cfgfileavailable ) mass_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, &gamepadstate );
				else mass_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, NULL, &gamepadstate );
				putchar( 10 );
				Connected = -1;
				gamepadCheckCooldown = 0;
			}
			if ( !TempBannedKeys[0x32] & kPressed( 0x32 ) | !TempBannedKeys[0x62] & kPressed( 0x62 ) ) {
				releaseallthekeys;
				Cooldowns[0x32] = 10;
				TempBannedKeys[0x32] = 1;
				Cooldowns[0x62] = 10;
				TempBannedKeys[0x62] = 1;
				if ( cfgfileavailable ) solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, configurationfile, &gamepadstate, buttonbit );
				else solo_assigner( translations, labels, PermBannedKeys, TempBannedKeys, Cooldowns, NULL, &gamepadstate, buttonbit );
				Connected = -1;
				gamepadCheckCooldown = 0;
			}

		}

		if ( gamepadCheckCooldown < 1 ) {

			switch ( XInputGetState( 0, &gamepadstate ) ) {

			case ERROR_SUCCESS:

				if ( Connected != 1 ) {
					previousPacketNumber = gamepadstate.dwPacketNumber - 1;
					puts( "Gamepad is connected!\n" );
					SetConsoleTitle( titleworking );
					Connected = 1;
				}

				if ( previousPacketNumber != gamepadstate.dwPacketNumber ) {

					inpcount = 0;
					releasecount = 0;

					for ( int i = 0; i < 6; i++ ) {
						if ( translations[i] && ( ( gamepadstate.Gamepad.wButtons & buttonbit[i] ) == buttonbit[i] ) ) {
							inp[inpcount].ki.wVk = translations[i];
							inpcount++;
							previouslyPressed[i] = 1;
						}
						else if ( previouslyPressed[i] ) {
							release[releasecount].ki.wVk = translations[i];
							releasecount++;
							previouslyPressed[i] = 0;
						}
					}

					if ( translations[6] && ( gamepadstate.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ) {
						inp[inpcount].ki.wVk = translations[6];
						inpcount++;
						previouslyPressed[6] = 1;
					}
					else if ( previouslyPressed[6] ) {
						release[releasecount].ki.wVk = translations[6];
						releasecount++;
						previouslyPressed[6] = 0;
					}

					if ( translations[7] && ( gamepadstate.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ) {
						inp[inpcount].ki.wVk = translations[7];
						inpcount++;
						previouslyPressed[7] = 1;
					}
					else if ( previouslyPressed[7] ) {
						release[releasecount].ki.wVk = translations[7];
						releasecount++;
						previouslyPressed[7] = 0;
					}

					for ( int i = 8; i < 16; i++ ) {
						if ( translations[i] && ( ( gamepadstate.Gamepad.wButtons & buttonbit[i] ) == buttonbit[i] ) ) {
							inp[inpcount].ki.wVk = translations[i];
							inpcount++;
							previouslyPressed[i] = 1;
						}
						else if ( previouslyPressed[i] ) {
							release[releasecount].ki.wVk = translations[i];
							releasecount++;
							previouslyPressed[i] = 0;
						}
					}

					if ( gPad( L, X ) * gPad( L, X ) + gPad( L, Y ) * gPad( L, Y ) > LeftStkThreshold ) {
						if ( gPad( L, Y ) > 2 * CustomThreshold ) Accumulation[0][0] += gPad( L, Y ) - CustomThreshold;
						else if ( gPad( L, Y ) < -2 * CustomThreshold ) Accumulation[0][1] -= gPad( L, Y ) + CustomThreshold;
						if ( gPad( L, X ) > 2 * CustomThreshold ) Accumulation[0][2] += gPad( L, X ) - CustomThreshold;
						else if ( gPad( L, X ) < -2 * CustomThreshold ) Accumulation[0][3] -= gPad( L, X ) + CustomThreshold;
					}

					if ( gPad( R, X ) * gPad( R, X ) + gPad( R, Y ) * gPad( R, Y ) > RightStkThreshold ) {
						if ( gPad( R, Y ) > 2 * CustomThreshold ) Accumulation[1][0] += gPad( R, Y ) - CustomThreshold;
						else if ( gPad( R, Y ) < -2 * CustomThreshold ) Accumulation[1][1] -= gPad( R, Y ) + CustomThreshold;
						if ( gPad( R, X ) > 2 * CustomThreshold ) Accumulation[1][2] += gPad( R, X ) - CustomThreshold;
						else if ( gPad( R, X ) < -2 * CustomThreshold ) Accumulation[1][3] -= gPad( R, X ) + CustomThreshold;
					}

					for ( int i = 0; i < 2; i++ )
						for ( int j = 0; j < 4; j++ ) {
							if ( Accumulation[i][j] > 32767 - CustomThreshold ) {
								Accumulation[i][j] -= 32767 - CustomThreshold;
								if ( translations[16 + 4 * i + j] ) {
									inp[inpcount].ki.wVk = translations[16 + 4 * i + j];
									inpcount++;
									previouslyPressed[16 + 4 * i + j] = 1;
								}
							}
							else if ( previouslyPressed[16 + 4 * i + j] ) {
								release[releasecount].ki.wVk = translations[16 + 4 * i + j];
								releasecount++;
								previouslyPressed[16 + 4 * i + j] = 0;
							}
						}

					SendInput( releasecount, release, sizeof * release );

				}

				SendInput( inpcount, inp, sizeof * inp );
				break;

			case ERROR_DEVICE_NOT_CONNECTED:

				if ( Connected ) {
					gamepadCheckCooldown = 60;
					releaseallthekeys;
					puts( "Gamepad is disconnected.\n" );
					SetConsoleTitle( titleidle );
					Connected = 0;
				}
				break;

			default:

				if ( Connected ) {
					gamepadCheckCooldown = 60;
					releaseallthekeys;
					puts( "Gamepad is not connected and is in some unexpected error state... Please report the case to me in details, if possible.\n" );
					SetConsoleTitle( titleidle );
					Connected = 0;
				}

			}

		}
		else gamepadCheckCooldown--;

		CooldownHandler
		Sleep( 20 );
	}
	return 0;
}