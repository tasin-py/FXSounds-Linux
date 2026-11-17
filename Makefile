# FxSound DSP engine - Linux build (proof of concept)
#
#   make            build build/libfxdsp.a and build/fxprocess
#   make clean
#
# Upstream sources live untouched-as-possible in upstream/; Linux shims live in compat/.

CXX      ?= g++
CC       ?= gcc
ifeq ($(SAN),1)
BUILD    ?= build-san
OPT      := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
else
BUILD    ?= build
OPT      := -O2 -g
endif

DEFS     := -DUNICODE -D_UNICODE -DNDEBUG -D_LIB -DPT_NON_MFC -DDSPSOFT_TARGET -DPT_DSP_BUILD=PT_DSP_DFX -DFX_LINUX
# Headers are referenced with inconsistent case (Windows is case-insensitive).
# tools/gen_case_links.sh creates correctly-cased symlinks in $(BUILD)/ci.
INCDIRS  := compat $(BUILD)/ci \
            upstream/dsp/ptutil/include upstream/dsp/include upstream/dsp \
            upstream/audiopassthru/include
INCLUDES := $(addprefix -I,$(INCDIRS))
WARN     ?= -w
CFLAGS   += $(OPT) -MMD -MP -fPIC $(WARN) $(DEFS) $(INCLUDES) -include compat/fx_linux_compat.h
CXXFLAGS += $(OPT) -MMD -MP -fPIC -std=gnu++17 -fpermissive $(WARN) $(DEFS) $(INCLUDES) -include compat/fx_linux_compat.h

# DSP sources: same list as upstream/dsp/DfxDsp.vcxproj
DSP_SRCS := $(addprefix upstream/dsp/, \
  DfxDsp.cpp DfxDspEq.cpp DfxDspPreset.cpp DfxDspPrivate.cpp DfxDspRegistry.cpp \
  ptComSftDfx/Comsftwr.c ptComSftDfx/ComsftwrCPP.cpp \
  ptechDsp/Aural/Aural032/Auralp32.c ptechDsp/Aural/Aural0/Auralp.c \
  ptechDsp/Dly32/Dly832/dly8p32.c ptechDsp/Dly8/Dly8p.c \
  ptechDsp/Lex/Lex16/Lex16.c ptechDsp/Lex/Lex32/Lex32.c \
  ptechDsp/Maximizer/Maxi16/Maxi16.c ptechDsp/Maximizer/Maxi32/Maxi32.c \
  ptechDsp/Peq/Peq832/peq8p32.c ptechDsp/Peq/Peq8/peq8p.c \
  ptechDsp/Play/Play16/Play16.c ptechDsp/Play/Play32/Play32.c \
  ptechDsp/wide/Wide16/Wide16.c ptechDsp/wide/Wide32/Wide32.c \
  $(addprefix ptutil/COM/, Com.cpp Comeprom.cpp Comget.cpp ComMem.cpp Compass.cpp Comread.cpp Comwave.cpp Comwrite.cpp) \
  $(addprefix ptutil/dfxp/, dfxpComm.cpp dfxpEq.cpp dfxpGet.cpp dfxpInit.cpp dfxpProcess.cpp dfxpProcessClear.cpp \
     dfxpProcessInt.cpp dfxpProcessReal.cpp dfxpQnt.cpp dfxpQuit.cpp dfxpRegistryStandard.cpp dfxpSession.cpp \
     dfxpSet.cpp dfxpSpectrum.cpp dfxpUniversal.cpp) \
  ptutil/dfxSharedUtil/dfxSharedUtil.cpp \
  $(addprefix ptutil/DspUtil/BinauralSync/, BinauralSynGet.cpp BinauralSynInit.cpp BinauralSynProcess.cpp BinauralSynSet.cpp) \
  $(addprefix ptutil/DspUtil/GraphicEq/, GraphicEqGet.cpp GraphicEqInit.cpp GraphicEqInitBands.cpp GraphicEqInitSections.cpp GraphicEqProcess.cpp GraphicEqSet.cpp) \
  $(addprefix ptutil/DspUtil/spectrum/, spectrumGet.cpp spectrumInit.cpp spectrumMessageValues.cpp spectrumProcess.cpp spectrumReset.cpp spectrumSet.cpp) \
  ptutil/DspUtil/SurroundSyn/SurroundSynInit.cpp ptutil/DspUtil/SurroundSyn/SurroundSynProcess.cpp \
  $(addprefix ptutil/Filt/, Fil12But.cpp FiltbiqdSos.cpp FiltCalcBiqd.cpp FiltCalcFilterResponse.cpp Filtpoly.cpp FiltRun.cpp) \
  ptutil/PRELST/Prelst.cpp \
  ptutil/PWAV/PwavConvert.cpp \
  $(addprefix ptutil/Qnt/, Qnt.cpp Qnt2But.cpp QntitoBoostCut.cpp Qntitol.cpp Qntitor.cpp Qntitor2.cpp Qntrtoi.cpp Qntrtol.cpp Qntrtor.cpp) \
  ptutil/realSample/realSampleForceLegalValues.cpp \
  $(addprefix ptutil/SOS/, Sos.cpp SosGet.cpp SosProcess.cpp SosSet.cpp) \
  $(addprefix ptutil/VALS/, Vals.cpp Valscfg.cpp Valsfile.cpp Valsget.cpp Valsset.cpp Valswarp.cpp))

# Portable helper libraries from audiopassthru (the Windows audio routing itself is NOT built)
HELPER_SRCS := $(addprefix upstream/audiopassthru/src/, \
  MRY/Mry.cpp \
  $(addprefix MTH/, MthBuffer.cpp Mthcrypt.cpp Mthfreq.cpp MthLinearRegression.cpp MthRand.cpp MthRegion.cpp MthStat.cpp MthTrig.cpp MthUtil.cpp MthWarp.cpp) \
  $(addprefix pstr/, pstr.cpp pstrArray.cpp pstrArraySort.cpp pstrCompare.cpp pstrExtension.cpp pstrHidden.cpp pstrTimeDisplay.cpp pstrURL.cpp pstrUserLibraryFilename.cpp pstrWide.cpp pstrXML.cpp) \
  FILE/FileGeneral.cpp SLOUT/Slout.cpp \
  reg/regWithKeyname.cpp reg/regWithoutKeyname.cpp reg/regRecursiveDelete.cpp)
# Not built (Windows-only and not needed by the DSP): FILE/FileDate.cpp, ptime/Ptime.cpp,
# AudioPassthru/*, sndDevices/*, operatingSystem/*. The reg/* wrappers run on compat/registry.cpp.

# Linux replacements for Windows-only pieces (registry, etc.)
COMPAT_SRCS := $(wildcard compat/*.cpp)

LIB_SRCS := $(DSP_SRCS) $(HELPER_SRCS) $(COMPAT_SRCS)
LIB_OBJS := $(patsubst %,$(BUILD)/obj/%.o,$(LIB_SRCS))

.PHONY: all clean caselinks engine
all: $(BUILD)/fxprocess $(BUILD)/fxsound-engine
engine: $(BUILD)/fxsound-engine

caselinks: $(BUILD)/ci/.stamp
$(BUILD)/ci/.stamp: tools/gen_case_links.sh $(wildcard compat/*.h)
	@sh tools/gen_case_links.sh $(BUILD)/ci
	@touch $@

$(BUILD)/obj/%.c.o: %.c | $(BUILD)/ci/.stamp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/%.cpp.o: %.cpp | $(BUILD)/ci/.stamp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/libfxdsp.a: $(LIB_OBJS)
	ar rcs $@ $^

$(BUILD)/fxprocess: tools/fxprocess.cpp $(BUILD)/libfxdsp.a
	$(CXX) $(OPT) -std=gnu++17 -Iupstream/dsp/include $< $(BUILD)/libfxdsp.a -o $@ -lm -lpthread

# Live audio engine (PipeWire)
PW_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3)
PW_LIBS   := $(shell pkg-config --libs libpipewire-0.3)
ENGINE_FLAGS := $(OPT) -MMD -MP -fPIC -std=gnu++17 -Wall -Wno-missing-field-initializers -Wno-deprecated-declarations -Iupstream/dsp/include -Iengine $(PW_CFLAGS)

$(BUILD)/obj/engine/%.o: engine/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(ENGINE_FLAGS) -c $< -o $@

$(BUILD)/libfxengine.a: $(BUILD)/obj/engine/fx_engine.o
	ar rcs $@ $^

$(BUILD)/fxsound-engine: $(BUILD)/obj/engine/fxsound_engine_main.o $(BUILD)/libfxengine.a $(BUILD)/libfxdsp.a
	$(CXX) $(OPT) $^ -o $@ $(PW_LIBS) -lm -lpthread

-include $(BUILD)/obj/engine/*.d

clean:
	rm -rf build build-san

-include $(LIB_OBJS:.o=.d)
