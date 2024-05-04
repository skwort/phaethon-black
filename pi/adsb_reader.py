import logging
import requests
import subprocess
import threading
import time


ADSB_DATA_SERVER_ADDRESS = "http://localhost:8080/data.json"
DUMP1090_PATH = "../../dump1090/dump1090"


class LoggerThread(threading.Thread):
    """
    A thread class for redirecting stderr output of a subprocess to the main
    process's logger.

    Args:
        process (subprocess.Popen): The subprocess whose stderr output is to
        be redirected.

    Attributes:
        process (subprocess.Popen): The subprocess whose stderr output is being
            redirected.
        stop_event (threading.Event): An event to signal the thread to stop.

    Methods:
        run(): The main method of the thread. Continuously reads stderr output
            from the subprocess and logs it using the main process's logger.
        stop(): Signals the thread to stop gracefully.
    """
    def __init__(self, process: subprocess.Popen):
        super().__init__()
        self.process = process
        self.stop_event = threading.Event()
        self.log = logging.getLogger("dump1090")

    def run(self):
        """ Thread function which periodically reads stderr of self.process """
        while True:
            if self.stop_event.is_set():
                break

            stderr_output = self.process.stderr.readline().decode()
            if stderr_output:
                self.log.info(stderr_output.strip())

            time.sleep(0.01)

    def stop(self):
        """ Method for stopping the thread gracefully """
        self.log.info("Stopping logger thread.")
        self.stop_event.set()


def get_dump1090_data():
    """
    Get ADS-B data from the dump1090 server.

    Returns:
        requests.Response or None: The response object containing ADS-B data if
        successful, or None if the request fails or encounters a connection
        error.

    The function sends a GET request to the ADS-B data server specified by the
    global variable ADSB_DATA_SERVER_ADDRESS. If the request is successful
    (status code 200), the response object containing the data is returned.
    Otherwise, None is returned.
    """
    try:
        response = requests.get(ADSB_DATA_SERVER_ADDRESS)

        if response.status_code != 200:
            logging.error("Unable to get ADSB data. "
                          f"Error {response.status_code}")
            return None

        return response

    except requests.exceptions.ConnectionError:
        logging.error("Connection error.")
        return None


def is_same_adsb(a: dict, b: dict) -> bool:
    """
    Check if two ADS-B packets have the same location and altitude.

    Args:
        a (dict): The first ADS-B packet.
        b (dict): The second ADS-B packet.

    Returns:
        bool: True if packets have the same lat, lon, and alt. False otherwise.
    """
    same_lat = a["lat"] == b["lat"]
    same_long = a["lon"] == b["lon"]
    same_alt = a["altitude"] == b["altitude"]
    if same_lat and same_long and same_alt:
        return True

    return False


def mainloop(dump1090: subprocess.Popen):

    adsb_cache = {}

    while True:
        # Poll the child
        if dump1090.poll() is not None:
            logging.error(f"dump1090 exited with code {dump1090.returncode}")
            break

        # GET the ADSB data
        response = get_dump1090_data()
        if response is None:
            time.sleep(0.05)
            continue

        # TODO: Encode data with protobuf and send to base node via UART
        data = response.json()
        for d in data:
            h = d["hex"]
            if h not in adsb_cache or not is_same_adsb(d, adsb_cache[h]):
                logging.info(f"New ADSB packet for {h}")
                adsb_cache[h] = d

        # Sleep for 50ms
        time.sleep(0.05)


def main():
    # Start the dump1090 child process
    dump1090 = subprocess.Popen([DUMP1090_PATH, "--net"],
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE)

    # Start the logger thread for re-routing dump1090 stderr to main process
    dump1090_logger = LoggerThread(dump1090)
    dump1090_logger.start()

    # Give dump1090 time to start
    time.sleep(3)

    try:
        logging.info("Starting main loop")
        mainloop(dump1090)

    except KeyboardInterrupt:
        logging.info("Keyboard interrupt received. Terminating dump1090")
        dump1090_logger.stop()
        dump1090_logger.join()
        dump1090.terminate()
        logging.info("Exiting")


if __name__ == "__main__":
    logging.basicConfig(encoding="utf-8", level=logging.INFO)
    main()
