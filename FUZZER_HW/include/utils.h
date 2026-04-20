#define SAFEFREE(_freemeptr) do{free(_freemeptr); _freemeptr = NULL;}while(0)
#define MIN(_xval, _yval) ((_xval) < (_yval) ? (_xval) : (_yval))
#define MAX(_xval, _yval) ((_xval) > (_yval) ? (_xval) : (_yval))