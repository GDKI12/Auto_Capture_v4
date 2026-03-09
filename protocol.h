#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtEndian>
#include <QByteArray>
#include <QString>

#pragma pack(push, 1)
struct PacketHeader
{
    char magic[4];
    quint8 type;
    quint8 reserved[3];
    quint64 payloadLenBE;
};

#pragma pack(pop)

static constexpr int HEADER_SIZE = int(sizeof(PacketHeader));

enum PacketType : quint8
{
    META = 0,
    DATA = 1,
    EOF_ = 2,
    ERROR_ = 3
};

inline QByteArray makePacket(PacketType type, const QByteArray& payload)
{
    PacketHeader h;
    memcpy(h.magic, "TSLA", 4);

    h.type = quint8(type);
    h.reserved[0] = h.reserved[1] = h.reserved[2] = 0;
    const quint64 len = quint64(payload.size());
    h.payloadLenBE = qToBigEndian(len);

    QByteArray out;
    out.resize(HEADER_SIZE);
    memcpy(out.data(), &h, HEADER_SIZE);
    out += payload;

    return out;
}

inline bool peekHeader(const QByteArray& buf, PacketHeader& outHeader, quint64& outPayloadLen)
{
    if(buf.size() < HEADER_SIZE)
        return false;
    memcpy(&outHeader, buf.constData(), HEADER_SIZE);

    if(memcmp(outHeader.magic, "TSLA", 4) != 0)
        return false;
    outPayloadLen = qFromBigEndian(outHeader.payloadLenBE);
    return true;
}

#endif // PROTOCOL_H
