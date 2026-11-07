import logging
import time

from arduino.app_utils import App, Bridge
from arduino.app_bricks.web_ui import WebUI

logger = logging.getLogger("folloq")
logging.basicConfig(level=logging.INFO)

web_ui = WebUI()
_fail_count = 0
_latest = {"error": "no data yet"}
_settings = {"speed": 35, "boost": False}


MODES = ["idle", "follow", "test", "manual"]
TEST_STATES = ["none", "running", "done", "aborted"]
STEP_NAMES = ["Left forward", "Left backward", "Right forward", "Right backward", "Both forward",
              "Both backward", "Spin left", "Spin right", "Ramp forward", "Full 100% fwd"]


def parse_state(raw: str) -> dict:
    # mode,test,step,pause, s1..s5, n1..n5, pos, found, cmdL, cmdR, batRaw, batMv, cap, speed
    p = raw.split(",")
    step = int(p[2])
    mode = MODES[int(p[0])]
    return {
        "mode": mode,
        "test": TEST_STATES[int(p[1])],
        "step": step,
        "total": len(STEP_NAMES),
        "step_name": STEP_NAMES[step - 1] if mode == "test" and 1 <= step <= len(STEP_NAMES) else "-",
        "pause": p[3] == "1",
        "sensors": [int(x) for x in p[4:9]],
        "strength": [int(x) for x in p[9:14]],
        "position": int(p[14]) / 1000.0,
        "found": p[15] == "1",
        "motor_l": int(p[16]),
        "motor_r": int(p[17]),
        "battery_raw": int(p[18]),
        "battery_v": int(p[19]) / 1000.0,
        "pwm_cap": int(p[20]),
        "speed": int(p[21]),
    }


def call(name, *args):
    try:
        return Bridge.call(name, *args)
    except Exception as e:
        logger.error("Bridge.call(%s) failed: %s", name, e)
        return None


def api_state():
    return _latest


def api_settings(speed: int = 35, boost: bool = False):
    _settings["speed"] = int(speed)
    _settings["boost"] = bool(boost)
    return dict(_settings)


def api_follow_start():
    logger.info("Follower start (speed=%s boost=%s)", _settings["speed"], _settings["boost"])
    return {"result": call("follow_start", _settings["speed"], _settings["boost"])}


def api_test_start():
    logger.info("Motor test start (boost=%s)", _settings["boost"])
    return {"result": call("test_start", _settings["boost"])}


def api_manual(left: int = 0, right: int = 0):
    left = max(-100, min(100, int(left)))
    right = max(-100, min(100, int(right)))
    return {"result": call("manual_drive", left, right, _settings["boost"])}


def api_stop():
    logger.warning("STOP")
    return {"result": call("stop_all")}


def api_cal_reset():
    logger.info("Calibration reset")
    return {"result": call("cal_reset")}


web_ui.expose_api("GET", "/state", api_state)
web_ui.expose_api("GET", "/settings", api_settings)  # ?speed=40&boost=false
web_ui.expose_api("POST", "/follow/start", api_follow_start)
web_ui.expose_api("POST", "/test/start", api_test_start)
web_ui.expose_api("GET", "/manual", api_manual)  # ?left=-50&right=50 (signed %, refreshed periodically)
web_ui.expose_api("POST", "/stop", api_stop)
web_ui.expose_api("POST", "/cal/reset", api_cal_reset)

_last_mode = None
_last_test = None
_last_log = 0.0


def loop():
    global _fail_count, _last_mode, _last_test, _last_log
    raw = call("get_state")
    if not raw:
        _fail_count += 1
        if _fail_count % 20 == 1:
            logger.warning("No state from sketch (%d consecutive failures)", _fail_count)
        _latest.clear()
        _latest["error"] = "no data from MCU"
        time.sleep(0.2)
        return
    _fail_count = 0
    try:
        st = parse_state(str(raw))
    except Exception as e:
        logger.error("Bad state string %r: %s", raw, e)
        time.sleep(0.2)
        return

    if st["mode"] != _last_mode or st["test"] != _last_test:
        logger.info("mode=%s test=%s", st["mode"], st["test"])
        _last_mode, _last_test = st["mode"], st["test"]
    now = time.time()
    if now - _last_log > 2:
        _last_log = now
        logger.info("sensors=%s line=%s pos=%.2f motors=%d/%d battery=%.2fV mode=%s",
                    st["sensors"], st["found"], st["position"], st["motor_l"], st["motor_r"],
                    st["battery_v"], st["mode"])

    _latest.clear()
    _latest.update(st)
    time.sleep(0.05)


App.run(user_loop=loop)
