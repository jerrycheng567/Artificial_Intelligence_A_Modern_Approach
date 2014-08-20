#ifndef CSP_HPP
#define CSP_HPP
#include <set>
#include <vector>
#include <cassert>
#include <algorithm>
#include <map>
#include <list>
#include <boost/any.hpp>
#include <boost/iterator/filter_iterator.hpp>
template< typename VARIABLE_ID_T, typename VARIABLE_T >
struct constraint
{
	std::vector< VARIABLE_ID_T > related_var;
	std::function< bool( std::vector< std::reference_wrapper< const VARIABLE_T > > ) > predicate;
	constraint( const std::vector< VARIABLE_ID_T > & v, const std::function< bool( std::vector< std::reference_wrapper< const VARIABLE_T > > ) > & f ) :
		related_var( v ), predicate( f ) { }
	bool operator ( )( const std::map< VARIABLE_ID_T, VARIABLE_T > & partial_assignment ) const
	{
		std::vector< std::reference_wrapper< const VARIABLE_T > > arg;
		arg.reserve( related_var.size( ) );
		for ( const VARIABLE_ID_T & i : related_var )
		{
			auto it = partial_assignment.find( i );
			if ( it == partial_assignment.end( ) ) { return true; }
			arg.push_back( it->second );
		}
		return predicate( arg );
	}
};

template< typename VARIABLE_ID_T, typename VARIABLE_T >
constraint< VARIABLE_ID_T, VARIABLE_T > make_constraint(
		const std::vector< VARIABLE_ID_T > & v,
		const std::function< bool( std::vector< std::reference_wrapper< const VARIABLE_T > > ) > & f )
{ return constraint< VARIABLE_ID_T, VARIABLE_T >( v, f ); }

template< typename VARIABLE_ID_T, typename VARIABLE_T, typename OUTITER >
OUTITER backtracking_search(
		std::map< VARIABLE_ID_T, std::list< VARIABLE_T > > variable,
		std::map< VARIABLE_ID_T, VARIABLE_T > & partial_assignment,
		std::set< VARIABLE_ID_T > modify_variable,
		size_t generalized_arc_consistency_upperbound,
		std::list< constraint< VARIABLE_ID_T, VARIABLE_T > > & constraint_set, OUTITER result )
{
	assert( partial_assignment.size( ) <= variable.size( ) );
	if ( modify_variable.size( ) == 1 )
	{
		variable.erase( * modify_variable.begin( ) );
		auto it = partial_assignment.find( * modify_variable.begin( ) );
		assert( it != partial_assignment.end( ) );
		variable.insert( std::make_pair( * modify_variable.begin( ), std::list< VARIABLE_T >( { it->second } ) ) );
	}
	assert ( !
			std::any_of(
				constraint_set.begin( ),
				constraint_set.end( ),
				[&]( const constraint< VARIABLE_ID_T, VARIABLE_T > & con ) { return ! con( partial_assignment ); } ) );
	if ( partial_assignment.size( ) == variable.size( ) )
	{
		* result = partial_assignment;
		++result;
		return result;
	}
	while ( ! modify_variable.empty( ) )
	{
		VARIABLE_ID_T current = * modify_variable.begin( );
		modify_variable.erase( current );
		std::for_each(
					constraint_set.begin( ),
					constraint_set.end( ),
					[&]( const constraint< VARIABLE_ID_T, VARIABLE_T > & con )
					{
						if ( con.related_var.size( ) <= generalized_arc_consistency_upperbound &&
							 con.related_var.size( ) >= 2 &&
							 std::count( con.related_var.begin( ), con.related_var.end( ), current ) != 0 )
						{
							std::vector< std::reference_wrapper< std::list< VARIABLE_T > > > parameters;
							parameters.reserve( con.related_var.size( ) );
							std::for_each(
										con.related_var.begin( ),
										con.related_var.end( ),
										[&]( const VARIABLE_ID_T & t )
										{
											auto it = variable.find( t );
											assert( it != variable.end( ) );
											parameters.push_back( it->second );
										} );
							auto shrink_domain =
									[&]( const size_t shrink_position )
									{
										bool arc_prune = false;
										for (
												auto it = parameters[ shrink_position ].get( ).begin( );
												it != parameters[ shrink_position ].get( ).end( );
												[&]( )
												{
													if( arc_prune == false ) { ++it; }
													else { arc_prune = false; }
												}( ) )
										{
											bool need_var = false;
											auto generate =
												[&]( const auto & self, std::vector< std::reference_wrapper< const VARIABLE_T > > & arg )
												{
													if ( need_var ) { return; }
													if ( arg.size( ) == parameters.size( ) )
													{ if ( con.predicate( arg ) == true ) { need_var = true; } }
													else if ( arg.size( ) == shrink_position )
													{
														arg.push_back( * it );
														self( self, arg );
														arg.pop_back( );
													}
													else
													{
														assert( arg.size( ) != shrink_position );
														for ( const VARIABLE_T & i : parameters[ arg.size( ) ].get( ) )
														{
															arg.push_back( i );
															self( self, arg );
															arg.pop_back( );
															if ( need_var ) { return; }
														}
													}
												};
											std::vector< std::reference_wrapper< const VARIABLE_T > > arg;
											generate( generate, arg );
											if ( need_var == false )
											{
												modify_variable.insert( con.related_var[ shrink_position ] );
												it = parameters[ shrink_position ].get( ).erase( it );
												arc_prune = true;
											}
										}
									};
							for ( size_t i = 0; i < parameters.size( ); ++i )
							{ shrink_domain( i ); }
						}
					} );
	}
	const std::pair< VARIABLE_ID_T, std::list< VARIABLE_T > > & next_element = * std::min_element(
				variable.begin( ),
				variable.end( ),
				[&]( const std::pair< VARIABLE_ID_T, std::list< VARIABLE_T > > & l, const std::pair< VARIABLE_ID_T, std::list< VARIABLE_T > > & r )
				{
					return ( partial_assignment.count( l.first ) == 0 ? l.second.size( ) : std::numeric_limits< size_t >::max( ) ) <
							( partial_assignment.count( r.first ) == 0 ? r.second.size( ) : std::numeric_limits< size_t >::max( ) );
				} );
	assert( partial_assignment.count( next_element.first ) == 0 );
	if ( next_element.second.empty( ) )
	{
		std::vector< VARIABLE_ID_T > assigned_neighbor_ID;
		std::vector< VARIABLE_T > assigned_neighbor_value;
		//constraint_set.push_back( make_constraint( assigned_neighbor_ID, [=](){} ) );
	}
	for ( const VARIABLE_T & t : next_element.second )
	{
		auto ret = partial_assignment.insert( std::make_pair( next_element.first, t ) );
		assert( ret.second );
		result = backtracking_search(
					variable,
					partial_assignment,
					{ next_element.first },
					generalized_arc_consistency_upperbound,
					constraint_set,
					result );
		partial_assignment.erase( next_element.first );
	}
	return result;
}

template< typename VARIABLE_ID_T, typename VARIABLE_T, typename OUTITER >
OUTITER backtracking_search(
		std::map< VARIABLE_ID_T, std::list< VARIABLE_T > > variable,
		size_t generalized_arc_consistency_upperbound,
		std::list< constraint< VARIABLE_ID_T, VARIABLE_T > > constraint_set, OUTITER result )
{
	bool do_return = false;
	std::for_each(
				constraint_set.begin( ),
				constraint_set.end( ),
				[&]( const constraint< VARIABLE_ID_T, VARIABLE_T > & con )
				{
					if ( con.related_var.empty( ) )
					{ if ( ! con.predicate( { } ) ) { do_return = true; } }
					else if ( con.related_var.size( ) == 1 )
					{
						auto element = variable.find( con.related_var.front( ) );
						assert( element != variable.end( ) );
						for ( auto it = element->second.begin( ); it != element->second.end( ); ++it )
						{ if ( ! con.predicate( { * it } ) ) { it = element->second.erase( it ); } }
					}
				} );
	if ( do_return ) { return result; }
	std::set< VARIABLE_ID_T > all_element;
	std::map< VARIABLE_ID_T, VARIABLE_T > ass;
	std::for_each( variable.begin( ), variable.end( ), [&]( const std::pair< VARIABLE_ID_T, std::list< VARIABLE_T > > & p ){ all_element.insert( p.first ); } );
	constraint_set.remove_if( []( const constraint< VARIABLE_ID_T, VARIABLE_T > & t ){ return t.related_var.size( ) < 2; } );
	return backtracking_search(
				variable,
				ass,
				all_element,
				generalized_arc_consistency_upperbound,
				constraint_set,
				result );
}

#endif // CSP_HPP
