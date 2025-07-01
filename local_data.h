const char* ssid = "router_name";
const char* password = "router_password";
char *diadelasemana[] = { "dom", "lun", "mar", "mie", "jue",  "vie", "sab"};
char *fullweekday[] = { "Domingo", "Lunes", "Martes", "Miercoles", "Jueves",  "Viernes", "Sabado"};

struct datos_loc
{
    String lat;
    String lon;
    String nom;
};

datos_loc ciudad[3] = {
  {"36.73038", "-4.43499", "MALAGA"},
  {"37.02485", "-4.57297", "ANTEQUERA"},
  {"40.43810", "-3.84434", "MADRID"}
};

