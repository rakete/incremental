from PandaModules import *
from DirectSessionGlobal import *
from IntervalGlobal import *

import Mopath

boat = loader.loadModel('models/directmodels/smiley')
boat.reparentTo(render)

dock = loader.loadModel('models/directmodels/smiley')
dock.reparentTo(render)

sound = loader.loadSound('phase_6/audio/sfx/SZ_DD_waterlap.mp3')

mp = Mopath.Mopath()
mp.loadFile(Filename('phase_6/paths/dd-e-w'))

# Set up the boat
boatMopath = MopathInterval(mp, boat, 'boatpath')
boatTrack = Track([boatMopath], 'boattrack')
BOAT_START = boatTrack.getIntervalStartTime('boatpath')
BOAT_END = boatTrack.getIntervalEndTime('boatpath')

# Make the dock lerp up so that it's up when the boat reaches the end of
# its mopath
<<<<<<< IntervalTest.py
dockLerp = LerpPosHprInterval('lerp', dock,
                              Point3(0, 0, -5),
                              hpr = Vec3(0, 0, 0),
                              5.0)
=======
pos = Point3(0, 0, -5)
hpr = Vec3(0, 0, 0)
dockLerp = LerpPosHprInterval(dock, 5.0, pos, hpr, name='dock-lerp')
>>>>>>> 1.12
# We need the dock's state to be defined before the lerp
dockPos = PosHprInterval(dock, dock.getPos(), dock.getHpr(), 1.0, 'dockpos')
dockUpTime = BOAT_END - dockLerp.getDuration()
hpr2 = Vec3(90.0, 90.0, 90.0)
dockLerp2 = LerpHprInterval(dock, 3.0, hpr2, name='hpr-lerp')
dockTrack = Track([dockLerp2, dockPos, dockLerp], 'docktrack')
dockTrack.setIntervalStartTime('lerp', dockUpTime)
dockTrack.setIntervalStartTime('hpr-lerp', BOAT_START)

# Start the water sound 5 seconds after the boat starts moving
waterStartTime = BOAT_START + 5.0
waterSound = SoundInterval(sound, name='watersound')
soundTrack = Track([waterSound], 'soundtrack')
soundTrack.setIntervalStartTime('watersound', waterStartTime)

# Throw an event when the water track ends
eventTime = soundTrack.getIntervalEndTime('watersound')
waterDone = EventInterval('water-is-done')
waterEventTrack = Track([waterDone])
waterEventTrack.setIntervalStartTime('water-is-done', eventTime)

mtrack = MultiTrack([boatTrack, dockTrack, soundTrack, waterEventTrack])
mtrack.printParams()

def handleWaterDone():
    print 'water is done'

messenger.accept('water-is-done', 1, handleWaterDone)
