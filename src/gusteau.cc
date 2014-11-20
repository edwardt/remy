#include <algorithm>
#include <limits>
#include <cmath>

#include "gusteau.hh"

using namespace std;

Gusteau::Gusteau( const uint64_t current_time )
  :  _memory(),
     _packets_sent( 0 ),
     _packets_received( 0 ),
     _last_send_time( current_time ),
     _max_receive_ratio( 1.0 ),
     _the_window( 0 ),
     _intersend_time( 0 ),
     _flow_id( 0 ),
     _largest_ack( -1 ),
     _flow_start( 0 )
{
}

/* helper function */
double calculate_caution( const double & metric ) {
  if( metric > 1.5 ) {
    return pow( metric, 2 ) / 5 + .1;
  } else {
    return pow( metric, 2 ) / 50 + .1;
  }
}

void Gusteau::packets_received( const vector< Packet > & packets ) {
  _packets_received += packets.size();

  /* Assumption: There is no reordering */
  _largest_ack = max( packets.at( packets.size() - 1 ).seq_num, _largest_ack );
  _memory.packets_received( packets, _flow_id );

  /* Approximate the degree of multiplexing (or the link rate?). 
     (RemyCCs probably do not do this.) */
  double receive_ratio = 1.0;
  if( _memory.field( 0 ) > 0 ) {
    receive_ratio = _memory.field( 1 ) / _memory.field ( 0 );
    _max_receive_ratio = max( receive_ratio, _max_receive_ratio );

    assert( _max_receive_ratio >= 1.0 );
  }

  /* RTT ratio threshold */
  if( _memory.field( 2 ) <= 1.01  and _memory.field( 2 ) >= 1.0 ) {
    /* Gradually allow more sending */
    _the_window += 35;

    /* Queue is small-- send faster! 
       Increase rate more cautiously as we approach smaller sewma values
       so as not to flood the queue. */
    double caution = calculate_caution( _max_receive_ratio );
    _intersend_time = _memory.field( 0 ) / ( _memory.field( 0 )/caution + 1 );
  } else if( _memory.field( 2 ) > 1.01 ) {
    /* Queue is too large; back off */
    //double caution = calculate_caution( _max_receive_ratio );
    _intersend_time =  _memory.field( 1 ) * max( 1.1, 1.1 / _max_receive_ratio );
  }
}

void Gusteau::reset( const double & )
{
  _memory.reset();
  _last_send_time = 0;
  _the_window = 0;
  _intersend_time = 0;
  _flow_id++;
  _largest_ack = _packets_sent - 1; /* Assume everything's been delivered */
  _flow_start = _largest_ack;
  assert( _flow_id != 0 );
  _max_receive_ratio = 1.0;
}

double Gusteau::next_event_time( const double & tickno ) const
{
  if ( _packets_sent < _largest_ack + 1 + _the_window ) {
    if ( _last_send_time + _intersend_time <= tickno ) {
      return tickno;
    } else {
      return _last_send_time + _intersend_time;
    }
  } else {
    /* window is currently closed */
    return std::numeric_limits<double>::max();
  }
}