# Theus Game

A chest-worn motion controller and a browser runner game. The player's real jump and crawl drive the character, and the state of a run is mirrored onto a panel worn on the body.

## Language

### Sensing and intent

**Sensor Module**:
The body-worn inertial unit that observes the player's movement.
_Avoid_: IMU, gyro, accelerometer

**Edge Classifier**:
The on-device component that turns Sensor Module readings into a stream of Intents.
_Avoid_: gesture recogniser, motion detector

**Intent**:
A meaning the player expressed with their body — a Jump, a Crawl, or the release of either.
_Avoid_: gesture, command, action

**Jump**:
The Intent to make the character leap.
_Avoid_: hop

**Crawl**:
The Intent to make the character duck low. It is expressed by holding a crouched posture, not by moving on hands and knees.
_Avoid_: duck, crouch, squat

**Recalibration**:
The act of re-baselining the Sensor Module against the player's neutral standing posture.
_Avoid_: calibration, re-zero

**Baseline**:
The neutral posture reference captured by a Recalibration, against which later movement is measured.
_Avoid_: zero, offset, neutral pose

**Threshold**:
A configurable motion value that separates an Intent from noise.
_Avoid_: sensitivity, gain

### Run and scoring

**Run**:
One attempt at the game, beginning when play starts and ending when the last Heart is lost.
_Avoid_: round, match

**Run State**:
Where a Run currently is: waiting to start, running, jumping, crawling, invulnerable, or dead.
_Avoid_: game state, status, mode

**Heart**:
A unit of the player's remaining health. A Run begins with three, and a collision costs one.
_Avoid_: HP, life, health

**Score**:
Points accumulated during the current Run, earned by distance travelled.
_Avoid_: points

**High Score**:
The best Score achieved across Runs.
_Avoid_: best, record

### Device and link

**Device**:
The complete chest-worn unit: Sensor Module, Status Board, controls, and battery.
_Avoid_: controller, remote, wearable

**Status Board**:
The panel on the Device that displays the state of a Run. It is read between Runs and by onlookers; the player cannot see it mid-Run.
_Avoid_: HUD, display, screen

**Link**:
The wireless connection between the Device and the game page.
_Avoid_: connection, cable, socket

**Session**:
The single game page currently bound to a Device. Only one exists at a time.
_Avoid_: client, tab, connection
