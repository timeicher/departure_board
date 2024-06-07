# departure_board
Arduino departure board using the Open-Data-Platform API

To use it you need to create the file arduino_secrets.h in your repository and define the following macros:
#define SECRET_SSID "yourwifi"
#define SECRET_PASS "yourpassword"
#define API_KEY "yourapikey"
#define XML_REQ "your xml request"

You can create an API key here: https://opentransportdata.swiss/en/dev-dashboard/
XML requests can be conveniently generated here: https://opentransportdata.swiss/en/cookbook/triprequest/
