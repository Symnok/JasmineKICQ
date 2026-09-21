// JasmineKICQ - an ICQ (OSCAR) client for Symbian Anna/Belle.
// Copyright (C) 2026 - GPL-2.0-or-later, see LICENSE.
#include "icqtypes.h"

namespace Icq
{

// Same order as Jasmine IM's xstatus.guids / icons, so the icon files carry over unchanged.
const char * const XStatusGuids[] = {
    "63627337A03F49FF80E5F709CDE0A4EE", "5A581EA1E580430CA06F612298B7E4C7", "83C9B78E77E74378B2C5FB6CFCC35BEC",
    "E601E41C33734BD1BC06811D6C323D81", "8C50DBAE81ED4786ACCA16CC3213C7B7", "3FB0BD36AF3B4A609EEFCF190F6A5A7F",
    "F8E8D7B282C4414290F810C6CE0A89A6", "80537DE2A4674A76B3546DFD075F5EC6", "F18AB52EDC57491D99DC6444502457AF",
    "1B78AE31FA0B4D3893D1997EEEAFB218", "61BEE0DD8BDD475D8DEE5F4BAACF19A7", "488E14898ACA4A0882AA77CE7A165208",
    "107A9A1812324DA4B6CD0879DB780F09", "6F4930984F7C4AFFA27634A03BCEAEA7", "1292E5501B644F66B206B29AF378E48D",
    "D4A611D08F014EC09223C5B6BEC6CCF0", "609D52F8A29A49A6B2A02524C5E9D260", "1F7A4071BF3B4E60BC324C5787B04CF1",
    "785E8C4840D34C65886F04CF3F3F43DF", "A6ED557E6BF744D4A5D4D2E7D95CE81F", "12D07E3EF885489E8E97A72A6551E58D",
    "BA74DB3E9E24434B87B62F6B8DFEE50F", "634F6BD8ADD24AA1AAB9115BC26D05A1", "01D8D7EEAC3B492AA58DD3D877E66B92",
    "2CE0E4E57C6443709C3A7A1CE878A7DC", "101117C9A3B040F981AC49E159FBD5D4", "160C60BBDD4443F39140050F00E6C009",
    "6443C6AF22604517B58CD7DF8E290352", "16F5B76FA9D240358CC5C084703C98FA", "631436FF3F8A40D0A5CB7B66E051B364",
    "B70867F538254327A1FFCF4CC1939797", "DDCF0EA971954048A9C6413206D6F280", "3FB0BD36AF3B4A609EEFCF190F6A5A7E",
    "E601E41C33734BD1BC06811D6C323D82", "D4E2B0BA334E4FA598D0117DBF4D3CC8", "0072D9084AD143DD91996F026966026F",
    "CD5643A2C94C4724B52CDC0124A1D0CD"
};
const int XStatusCount = sizeof(XStatusGuids) / sizeof(XStatusGuids[0]);

const char * const XStatusNames[] = {
    "shopping", "duck", "tired", "party", "beer", "think", "eating", "tv", "friends", "coffee",
    "music", "business", "camera", "funny", "phone", "games", "college", "sick", "sleep", "surfing",
    "internet", "engineering", "typing", "angry", "picnic", "ppc", "mobile", "man", "wc", "question",
    "way", "love", "smoke", "sex", "search", "diary", "rulove"
};

int xstatusIndex(const QString &guidHex)
{
    for (int i = 0; i < XStatusCount; ++i)
        if (guidHex.compare(QLatin1String(XStatusGuids[i]), Qt::CaseInsensitive) == 0) return i;
    return -1;
}

namespace
{
    struct QipMood { const char *guid; int status; };
    const QipMood qipMoods[] = {
        {"B7074378F50C777797775778502D0570", StatusDepress},
        {"B7074378F50C777797775778502D0575", StatusFfc},
        {"B7074378F50C777797775778502D0576", StatusHome},
        {"B7074378F50C777797775778502D0577", StatusWork},
        {"B7074378F50C777797775778502D0578", StatusLunch},
        {"B7074378F50C777797775778502D0579", StatusEvil}
    };
}

int qipStatusFromGuid(const QString &guidHex)
{
    for (unsigned i = 0; i < sizeof(qipMoods) / sizeof(qipMoods[0]); ++i)
        if (guidHex.compare(QLatin1String(qipMoods[i].guid), Qt::CaseInsensitive) == 0) return qipMoods[i].status;
    return 0;
}

QString qipGuidForStatus(int status)
{
    for (unsigned i = 0; i < sizeof(qipMoods) / sizeof(qipMoods[0]); ++i)
        if (qipMoods[i].status == status) return QLatin1String(qipMoods[i].guid);
    return QString();
}

}
