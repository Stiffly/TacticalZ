/****************************************************************************
** Meta object code from reading C++ file 'Menu.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.6)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../Menu.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Menu.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.6. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_Menu[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      14,    6,    5,    5, 0x08,
      35,    5,    5,    5, 0x08,
      59,    5,    5,    5, 0x08,
      80,    5,    5,    5, 0x08,
     104,    5,    5,    5, 0x08,
     120,    5,    5,    5, 0x08,
     140,    5,    5,    5, 0x08,
     161,    5,    5,    5, 0x08,
     182,    5,    5,    5, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_Menu[] = {
    "Menu\0\0checked\0ExportSelected(bool)\0"
    "ExportPathClicked(bool)\0AddClipClicked(bool)\0"
    "RemoveClipClicked(bool)\0ExportAll(bool)\0"
    "CancelClicked(bool)\0Button1Clicked(bool)\0"
    "Button2Clicked(bool)\0Button3Clicked(bool)\0"
};

void Menu::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        Menu *_t = static_cast<Menu *>(_o);
        switch (_id) {
        case 0: _t->ExportSelected((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 1: _t->ExportPathClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 2: _t->AddClipClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 3: _t->RemoveClipClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->ExportAll((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: _t->CancelClicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 6: _t->Button1Clicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 7: _t->Button2Clicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 8: _t->Button3Clicked((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData Menu::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject Menu::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_Menu,
      qt_meta_data_Menu, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &Menu::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *Menu::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *Menu::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_Menu))
        return static_cast<void*>(const_cast< Menu*>(this));
    return QWidget::qt_metacast(_clname);
}

int Menu::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
