
#include "benefactor.h"


Benefactor::Benefactor() {
 time = 0;
}

bool Benefactor::flowerpotOrToilet() {
    //0 flowerpot: 1 toilet.
    return false;
}

int Benefactor::findItemToFix(bool choice) {
    choice = this->flowerpotOrToilet();
    return 0;
}
