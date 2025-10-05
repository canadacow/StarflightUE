#pragma once

#include <string>

// TTS completely stubbed out for UE build
void InitTextToSpeech();
void SayText(std::string text, int raceNum);
void StopSpeech();