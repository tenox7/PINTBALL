# PINTBALL: SpaceCadetPinball for PINT 4.0 (Windows NT 4.0 on ARM), in plain Win32:
# GDI, waveOut, MCI, Win32 menus and dialogs. No SDL, no ImGui.
#
# Build with the PINT 4.0 SDK (its setenv.cmd, or the tenox7/pint40sdk Docker image):
#     nmake -f pint.mak
#
# The ImGui, SDL, debug overlay, font selection and EmbeddedData sources are
# not part of this build.

!include <win32.mak>

PROGRAM = PINTBALL.EXE

OBJS = control.obj fullscrn.obj gdrv.obj GroupData.obj high_score.obj loader.obj \
       maths.obj midi.obj nudge.obj options.obj partman.obj pb.obj proj.obj render.obj \
       score.obj Sound.obj SpaceCadetPinball.obj splash.obj TBall.obj TBlocker.obj TBumper.obj \
       TCircle.obj TCollisionComponent.obj TComponentGroup.obj TDemo.obj TDrain.obj \
       TEdgeManager.obj TEdgeSegment.obj TFlagSpinner.obj TFlipper.obj TFlipperEdge.obj \
       TGate.obj THole.obj timer.obj TKickback.obj TKickout.obj TLight.obj \
       TLightBargraph.obj TLightGroup.obj TLightRollover.obj TLine.obj TOneway.obj \
       TPinballComponent.obj TPinballTable.obj TPlunger.obj TPopupTarget.obj TRamp.obj \
       translations.obj TRollover.obj TSink.obj TSoloTarget.obj TSound.obj \
       TTableLayer.obj TTextBox.obj TTextBoxMessage.obj TTimer.obj TTripwire.obj TWall.obj \
       winmain.obj wintext.obj zdrv.obj

# The game uses no C++ exceptions, so there is no -EHsc. dynamic_cast needs
# the run-time types (-GR). The static C run-time (-MT) runs on every PINT 4.0.
PINTFLAGS = $(cflags) $(cvarsmt) -O2 -GR -DNDEBUG

all: $(PROGRAM)

# One cl for all the changed sources.
.cpp.obj::
    $(cc) $(PINTFLAGS) $<

$(OBJS): pch.h pintstl.h

SpaceCadetPinball.res: SpaceCadetPinball.rc resource.h Icon_pint.ico PB_MSGFT.bin
    $(rc) $(rcflags) $(rcvars) SpaceCadetPinball.rc

$(PROGRAM): $(OBJS) SpaceCadetPinball.res
    $(link) $(guilflags) -out:$(PROGRAM) $(OBJS) SpaceCadetPinball.res winmm.lib $(guilibsmt)

clean:
    -del *.obj SpaceCadetPinball.res $(PROGRAM)
