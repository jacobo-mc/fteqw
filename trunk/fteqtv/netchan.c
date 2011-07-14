/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "qtv.h"
#include <string.h>

#define curtime Sys_Milliseconds()


SOCKET NET_ChooseSocket(SOCKET sock[2], netadr_t *adr)
{
#ifdef AF_INET6
	if (((struct sockaddr *)adr)->sa_family == AF_INET6)
		return sock[1];
#endif
	return sock[0];
}

void NET_SendPacket(cluster_t *cluster, SOCKET sock, int length, void *data, netadr_t adr)
{
	int ret;

	ret = sendto(sock, data, length, 0, (struct sockaddr *)&adr, sizeof(struct sockaddr_in));
	if (ret < 0)
	{
		int er = qerrno;
		if (er == EWOULDBLOCK || er == EAGAIN)
			return;

		Sys_Printf(cluster, "udp send error %i (%s)\n", er, strerror(er));
	}
}

int Netchan_IsLocal (netadr_t adr)
{
	struct sockaddr_in *sadr = (struct sockaddr_in *)&adr;
	unsigned char *bytes;
	switch(((struct sockaddr *)&adr)->sa_family)
	{
	case AF_INET:
		bytes = (unsigned char *)&((struct sockaddr_in *)&adr)->sin_addr;
		if (bytes[0] == 127 &&
			bytes[1] == 0 &&
			bytes[2] == 0 &&
			bytes[3] == 1)
			return true;
		return false;
	case AF_INET6:
		bytes = (unsigned char *)&((struct sockaddr_in6 *)&adr)->sin6_addr;
		if (bytes[ 0] == 0 &&
			bytes[ 1] == 0 &&
			bytes[ 2] == 0 &&
			bytes[ 3] == 0 &&
			bytes[ 4] == 0 &&
			bytes[ 5] == 0 &&
			bytes[ 6] == 0 &&
			bytes[ 7] == 0 &&
			bytes[ 8] == 0 &&
			bytes[ 9] == 0 &&
			bytes[10] == 0 &&
			bytes[11] == 0 &&
			bytes[12] == 0 &&
			bytes[13] == 0 &&
			bytes[14] == 0 &&
			bytes[15] == 1)
			return true;
		return false;
	default:
		return false;
	}
}









#define	PACKET_HEADER	8

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16  qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher than the last reliable sequence, but without the correct even/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

If the sequence number is -1, the packet should be handled without a netcon.

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.

The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.


*/


/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand (cluster_t *cluster, SOCKET sock, netadr_t adr, int length, void *data)
{
	netmsg_t	send;
	unsigned char		send_buf[MAX_MSGLEN + PACKET_HEADER];

// write the packet header
	InitNetMsg (&send, send_buf, sizeof(send_buf));

	WriteLong (&send, -1);	// -1 sequence means out of band
	WriteData (&send, data, length);

// send the datagram
	NET_SendPacket (cluster, sock, send.cursize, send.data, adr);
}


/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint (cluster_t *cluster, SOCKET sock[], netadr_t adr, char *format, ...)
{
	va_list		argptr;
	char		string[8192];

	va_start (argptr, format);
#ifdef _WIN32
	_vsnprintf (string, sizeof(string) - 1, format, argptr);
	string[sizeof(string) - 1] = '\0';
#else
	vsnprintf (string, sizeof(string), format, argptr);
#endif // _WIN32
	va_end (argptr);

	Netchan_OutOfBand (cluster, NET_ChooseSocket(sock, &adr), adr, strlen(string), (unsigned char *)string);
}

/*
===============
Netchan_Init

===============
*/
void Netchan_Init (netadr_t adr)
{

}


/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup (SOCKET sock, netchan_t *chan, netadr_t adr, int qport, qboolean isclient)
{
	memset (chan, 0, sizeof(*chan));

	chan->sock = sock;
	memcpy(&chan->remote_address, &adr, sizeof(netadr_t));
	chan->qport = qport;
	chan->isclient = isclient;

	chan->last_received = curtime;

	InitNetMsg(&chan->message, chan->message_buf, sizeof(chan->message_buf));

	chan->message.allowoverflow = true;

	chan->rate = 1000.0f/2500;
}


/*
===============
Netchan_CanPacket

Returns true if the bandwidth choke isn't active
================
*/
qboolean Netchan_CanPacket (netchan_t *chan)
{
	unsigned int t;
	// unlimited bandwidth for local client
//	if (chan->remote_address.type == NA_LOOPBACK)
//		return true;

	t = curtime;
	if (chan->cleartime < t)
		return true;
	return false;
}


/*
===============
Netchan_CanReliable

Returns true if the bandwidth choke isn't
================
*/
qboolean Netchan_CanReliable (netchan_t *chan)
{
	if (chan->reliable_length)
		return false;			// waiting for ack
	return Netchan_CanPacket (chan);
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
void Netchan_Transmit (cluster_t *cluster, netchan_t *chan, int length, const void *data)
{
	unsigned int t;
	netmsg_t	send;
	unsigned char	send_buf[MAX_NQMSGLEN + PACKET_HEADER];
	qboolean		send_reliable;
	unsigned	w1, w2;

	if (chan->isnqprotocol)
	{
		int i;

		send.data = send_buf;
		send.maxsize = MAX_NQMSGLEN + PACKET_HEADER;
		send.cursize = 0;

		if (!chan->reliable_length && chan->message.cursize)
		{
			memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
			chan->reliable_length = chan->message.cursize;
			chan->reliable_start = 0;
			chan->message.cursize = 0;
		}

		i = chan->reliable_length - chan->reliable_start;
		if (i>0)
		{
			WriteLong(&send, 0);
			WriteLong(&send, SwapLong(chan->reliable_sequence));
			if (i > MAX_NQDATAGRAM)
				i = MAX_NQDATAGRAM;

			WriteData (&send, chan->reliable_buf+chan->reliable_start, i);
			if (length && send.cursize + length < send.maxsize)
			{	//throw the unreliable packet into the same one as the reliable (but not sent reliably)
				WriteData (&send, data, length);
				length = 0;
			}


			if (chan->reliable_start+i == chan->reliable_length)
				*(int*)send_buf = BigLong(NETFLAG_DATA | NETFLAG_EOM | send.cursize);
			else
				*(int*)send_buf = BigLong(NETFLAG_DATA | send.cursize);
			NET_SendPacket(cluster, chan->sock, send.cursize, send.data, chan->remote_address);

			if (chan->cleartime < curtime)
				chan->cleartime = curtime + (int)(send.cursize*chan->rate);
			else
				chan->cleartime += (int)(send.cursize*chan->rate);
		}

		//send out the unreliable (if still unsent)
		if (length)
		{
			WriteLong(&send, 0);
			WriteLong(&send, SwapLong(chan->outgoing_unreliable));
			chan->outgoing_unreliable++;

			WriteData (&send, data, length);

			*(int*)send_buf = BigLong(NETFLAG_UNRELIABLE | send.cursize);
			NET_SendPacket (cluster, chan->sock, send.cursize, send.data, chan->remote_address);

			if (chan->cleartime < curtime)
				chan->cleartime = (int)(curtime + send.cursize*chan->rate);
			else
				chan->cleartime += (int)(send.cursize*chan->rate);

			send.cursize = 0;
		}
		return;
	}






// check for message overflow
	if (chan->message.overflowed)
	{
		chan->drop = true;
//		printf ("%s:Outgoing message overflow\n"
//			, NET_AdrToString (chan->remote_address));
		return;
	}

// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence
	&& chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = true;

// if the reliable transmit buffer is empty, copy the current message out
	if (!chan->reliable_length && chan->message.cursize)
	{
		memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
		send_reliable = true;
	}

// write the packet header
	send.data = send_buf;
	send.maxsize = sizeof(send_buf);
	send.readpos = send.cursize = 0;

	w1 = chan->outgoing_sequence | (send_reliable<<31);
	w2 = chan->incoming_sequence | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;

	WriteLong (&send, w1);
	WriteLong (&send, w2);

	// send the qport if we are a client
	if (chan->isclient)
		WriteShort (&send, chan->qport);

// copy the reliable message to the packet first
	if (send_reliable)
	{
		WriteData (&send, chan->reliable_buf, chan->reliable_length);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}

// add the unreliable part if space is available
	if (send.maxsize - send.cursize >= length)
		WriteData (&send, data, length);

// send the datagram
//	i = chan->outgoing_sequence & (MAX_LATENT-1);
//	chan->outgoing_size[i] = send.cursize;
//	chan->outgoing_time[i] = curtime;

	NET_SendPacket (cluster, chan->sock, send.cursize, send.data, chan->remote_address);

	t = curtime;
	if (chan->cleartime < t)
		chan->cleartime = t + (int)(send.cursize*chan->rate);
	else
		chan->cleartime += (int)(send.cursize*chan->rate);
#ifndef CLIENTONLY
//	if (chan->sock == NS_SERVER && sv_paused.value)
//		chan->cleartime = curtime;
#endif

/*	if (showpackets.value)
		Com_Printf ("--> s=%i(%i) a=%i(%i) %i\n"
			, chan->outgoing_sequence
			, send_reliable
			, chan->incoming_sequence
			, chan->incoming_reliable_sequence
			, send.cursize);
*/
}




qboolean NQNetchan_Process(cluster_t *cluster, netchan_t *chan, netmsg_t *msg)
{
	int header;
	int sequence;
	int drop;

	msg->readpos = 0;

	header = SwapLong(ReadLong(msg));
	if (msg->cursize != (header & NETFLAG_LENGTH_MASK))
		return false;	//size was wrong, couldn't have been ours.

	if (header & NETFLAG_CTL)
		return false;	//huh?

	sequence = SwapLong(ReadLong(msg));

	if (header & NETFLAG_ACK)
	{
		if (sequence == chan->reliable_sequence)
		{
			chan->reliable_start += MAX_NQDATAGRAM;
			if (chan->reliable_start >= chan->reliable_length)
			{
				chan->reliable_length = 0;	//they got the entire message
				chan->reliable_start = 0;
			}
			chan->incoming_reliable_acknowledged = chan->reliable_sequence;
			chan->reliable_sequence++;

			chan->last_received = curtime;
		}
//		else if (sequence < chan->reliable_sequence)
//			Con_DPrintf("Stale ack recieved\n");
//		else if (sequence > chan->reliable_sequence)
//			Con_Printf("Future ack recieved\n");

		return false;	//don't try execing the 'payload'. I hate ack packets.
	}

	if (header & NETFLAG_UNRELIABLE)
	{
		if (sequence < chan->incoming_unreliable)
		{
//			Con_DPrintf("Stale datagram recieved\n");
			return false;
		}
		drop = sequence - chan->incoming_unreliable - 1;
		if (drop > 0)
		{
//			Con_DPrintf("Dropped %i datagrams\n", drop);
//			chan->drop_count += drop;
		}
		chan->incoming_unreliable = sequence;

		chan->last_received = curtime;

		chan->incoming_acknowledged++;
//		chan->good_count++;
		return 1;
	}
	if (header & NETFLAG_DATA)
	{
		int runt[2];
		//always reply. a stale sequence probably means our ack got lost.
		runt[0] = BigLong(NETFLAG_ACK | 8);
		runt[1] = BigLong(sequence);
		NET_SendPacket (cluster, chan->sock, 8, (void*)runt, chan->remote_address);

		chan->last_received = curtime;
		if (sequence == chan->incoming_reliable_sequence)
		{
			chan->incoming_reliable_sequence++;

			if (chan->in_fragment_length + msg->cursize-8 >= sizeof(chan->in_fragment_buf))
			{
				chan->drop = true;
				return false;
			}

			memcpy(chan->in_fragment_buf + chan->in_fragment_length, (char*)msg->data+8, msg->cursize-8);
			chan->in_fragment_length += msg->cursize-8;

			if (header & NETFLAG_EOM)
			{
				msg->cursize = 0;
				WriteData(msg, chan->in_fragment_buf, chan->in_fragment_length);
				chan->in_fragment_length = 0;
				msg->readpos = 0;
				return 2;	//we can read it now
			}
		}
//		else
//			Con_DPrintf("Stale reliable (%i)\n", sequence);
		return false;
	}

	return false;	//not supported.
}


/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
qboolean Netchan_Process (netchan_t *chan, netmsg_t *msg)
{
	unsigned		sequence, sequence_ack;
	unsigned		reliable_ack, reliable_message;

// get sequence numbers
	msg->readpos = 0;
	sequence = ReadLong (msg);
	sequence_ack = ReadLong (msg);

	// read the qport if we are a server
	if (!chan->isclient)
		ReadShort (msg);

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1<<31);
	sequence_ack &= ~(1<<31);

/*	if (showpackets.value)
		Com_Printf ("<-- s=%i(%i) a=%i(%i) %i\n"
			, sequence
			, reliable_message
			, sequence_ack
			, reliable_ack
			, net_message.cursize);
	*/

//
// discard stale or duplicated packets
//
	if (sequence <= (unsigned)chan->incoming_sequence)
	{
/*		if (showdrop.value)
			Com_Printf ("%s:Out of order packet %i at %i\n"
				, NET_AdrToString (chan->remote_address)
				,  sequence
				, chan->incoming_sequence);
		*/
		return false;
	}

//
// dropped packets don't keep the message from being used
//
/*	chan->dropped = sequence - (chan->incoming_sequence+1);
	if (chan->dropped > 0)
	{
		chan->drop_count += 1;

		if (showdrop.value)
			Com_Printf ("%s:Dropped %i packets at %i\n"
			, NET_AdrToString (chan->remote_address)
			, chan->dropped
			, sequence);
	}
*/


//
// if the current outgoing reliable message has been acknowledged
// clear the buffer to make way for the next
//
	if (reliable_ack == (unsigned)chan->reliable_sequence)
		chan->reliable_length = 0;	// it has been received

//
// if this message contains a reliable message, bump incoming_reliable_sequence
//
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
		chan->incoming_reliable_sequence ^= 1;

//
// the message can now be read from the current message pointer
// update statistics counters
//
//	chan->frame_latency = chan->frame_latency*OLD_AVG
//		+ (chan->outgoing_sequence-sequence_ack)*(1.0-OLD_AVG);
//	chan->frame_rate = chan->frame_rate*OLD_AVG
//		+ (curtime - chan->last_received)*(1.0-OLD_AVG);
//	chan->good_count += 1;

	chan->last_received = curtime;

	return true;
}
