
#pragma once

#include "BlockHandler.h"





class cBlockEndPortalFrameHandler :
	public cMetaRotator<cBlockHandler, 0x03,
	E_META_END_PORTAL_FRAME_ZM,
	E_META_END_PORTAL_FRAME_XP,
	E_META_END_PORTAL_FRAME_ZP,
	E_META_END_PORTAL_FRAME_XM
	>
{
public:
	cBlockEndPortalFrameHandler(BLOCKTYPE a_BlockType):
		cMetaRotator<cBlockHandler, 0x03,
		E_META_END_PORTAL_FRAME_ZM,
		E_META_END_PORTAL_FRAME_XP,
		E_META_END_PORTAL_FRAME_ZP,
		E_META_END_PORTAL_FRAME_XM
		>(a_BlockType)
	{
	}





	virtual bool GetPlacementBlockTypeMeta(
		cChunkInterface & a_ChunkInterface, cPlayer & a_Player,
		int a_BlockX, int a_BlockY, int a_BlockZ, eBlockFace a_BlockFace,
		int a_CursorX, int a_CursorY, int a_CursorZ,
		BLOCKTYPE & a_BlockType, NIBBLETYPE & a_BlockMeta
	) override
	{
		a_BlockType = m_BlockType;
		a_BlockMeta = YawToMetaData(a_Player.GetYaw());
		return true;
	}





	inline static NIBBLETYPE YawToMetaData(double a_Rotation)
	{
		a_Rotation += 90 + 45;  // So its not aligned with axis
		if (a_Rotation > 360)
		{
			a_Rotation -= 360;
		}

		if ((a_Rotation >= 0) && (a_Rotation < 90))
		{
			return E_META_END_PORTAL_FRAME_XM;
		}
		else if ((a_Rotation >= 180) && (a_Rotation < 270))
		{
			return E_META_END_PORTAL_FRAME_XP;
		}
		else if ((a_Rotation >= 90) && (a_Rotation < 180))
		{
			return E_META_END_PORTAL_FRAME_ZM;
		}
		else
		{
			return E_META_END_PORTAL_FRAME_ZP;
		}
	}





	virtual void OnPlaced(cChunkInterface & a_ChunkInterface, cWorldInterface & a_WorldInterface, int a_BlockX, int a_BlockY, int a_BlockZ, BLOCKTYPE a_BlockType, NIBBLETYPE a_BlockMeta) override
	{
		// E_META_END_PORTAL_EYE is the bit which signifies the eye of ender is in it.
		// LOG("PortalPlaced, meta %d", a_BlockMeta);
		if ((a_BlockMeta & E_META_END_PORTAL_EYE) == E_META_END_PORTAL_EYE)
		{
			// LOG("Location is %d %d %d", a_BlockX, a_BlockY, a_BlockZ);
			// Direction is the first two bits, masked by 0x3
			FindAndSetPortal(Vector3i(a_BlockX, a_BlockY, a_BlockZ), a_BlockMeta & 3, a_ChunkInterface, a_WorldInterface);
		}
	}





	enum EndPortalReturns
	{
		E_VALID_PORTAL_FRAME = 0,
		E_INVALID_PORTAL_FRAME = 1,
		E_NOT_PORTAL_FRAME = 2,
	};




	/** Returns false if portal cannot be made, true if portal was made. */
	bool FindAndSetPortal(Vector3i a_FirstFrame, NIBBLETYPE a_Direction, cChunkInterface & a_ChunkInterface, cWorldInterface & a_WorldInterface)
	{
		/*
		PORTAL FINDING ALGORITH
		=======================
		- Get clicked base block
		- Check diagonally (clockwise) for another portal block
			- if exists, and has eye, Continue. Abort if any are facing the wrong direction.
			- if doesn't exist, check horizontally (the block to the left of this block). Abort if there is no horizontal block.
		- After a corner has been met, traverse the portal clockwise, ensuring valid portal frames connect the rectangle.
		- Track the NorthWest Corner, and the dimensions.
		- If dimensions are valid, create the portal.
		*/

		// Make sure this is going clockwise.
		ASSERT(E_META_END_PORTAL_FRAME_ZP - E_META_END_PORTAL_FRAME_XP == 1);

		const int MIN_PORTAL_WIDTH = 3;
		const int MAX_PORTAL_WIDTH = 4;

		// Directions to use for the clockwise traversal.
		static const Vector3i Left[] =
		{
			{ 1, 0,  0},  // 0, South, left block is East  / XP
			{ 0, 0,  1},  // 1, West,  left block is South / ZP
			{-1, 0,  0},  // 2, North, left block is West  / XM
			{ 0, 0, -1},  // 3, East,  left block is North / ZM
		};
		static const Vector3i LeftForward[] =
		{
			{ 1, 0,  1},  // 0, South, left block is SouthEast / XP ZP
			{-1, 0,  1},  // 1, West,  left block is SouthWest / XM ZP
			{-1, 0, -1},  // 2, North, left block is NorthWest / XM ZM
			{ 1, 0, -1},  // 3, East,  left block is NorthEast / XP ZM
		};


		int Turns = 0;
		int Proper = 1;  // The first portal frame was already checked.
		Vector3i Former = a_FirstFrame;
		Vector3i Corner;  // The first and third corners.
		int Width[4] = {1, 1, 1, 1};

		while (Turns <= 4)
		{
			Vector3i Target;
			BLOCKTYPE TargetBlockType;
			NIBBLETYPE TargetBlockMeta;

			// Since we start on the first wall, get the corner, then cover each wall, we're effectively scanning 5 walls.
			if (Proper > (MAX_PORTAL_WIDTH * 5))
			{
				return false;
			}

			// (a_Direction + Turns) % 4 is the direction the former block is facing.
			Target = Former + LeftForward[(a_Direction + Turns) % 4];
			if (!a_ChunkInterface.GetBlockTypeMeta(Target, TargetBlockType, TargetBlockMeta))
			{
				// If the block couldn't be read, fail.
				return false;
			}

			// (a_Direction + Turns + 1) % 4 is the direction the corner block should face.
			if (IsValidFrame(TargetBlockType, TargetBlockMeta, (a_Direction + Turns + 1) % 4))
			{

				// When the former portal frame faces positive X, this corner will be XM, ZM.
				if ((a_Direction + Turns) % 4 == E_META_END_PORTAL_FRAME_XP)
				{
					// Remember the NW (XM, ZM) Corner
					// Relative to the previous frame, the portal should appear to the right of this portal frame.
					Corner = Target - Left[(a_Direction + Turns) % 4];
				}
				Former = Target;
				Turns++;
				Proper++;
				continue;
			}
			else if (IsPortalFrame(TargetBlockType))
			{
				// The Portal Frame existed, but was either missing the eye, or was facing the wrong direction.
				return false;
			}
			else
			{

				// Corner is not a portal block, check the left edge.
				Target = Former + Left[(a_Direction + Turns) % 4];
				if (!a_ChunkInterface.GetBlockTypeMeta(Target, TargetBlockType, TargetBlockMeta))
				{
					// If the block couldn't be read, fail.
					return false;
				}

				// The edge should face the same direction, (a_Direction + Turns) % 4.
				if (IsValidFrame(TargetBlockType, TargetBlockMeta, (a_Direction + Turns) % 4))
				{

					// After the first turn, start tracking length of frame edges (width of portal).
					if (Turns >= 1)
					{
						// X width stored at 0 and 2, Z width at 1 and 3.
						Width[(a_Direction + Turns) % 4]++;

						if (Width[(a_Direction + Turns) % 4] > MAX_PORTAL_WIDTH)
						{
							// Don't build a portal that is too long.
							return false;
						}
					}
					Former = Target;
					Proper++;
					continue;
				}
				else
				{
					// Neither the edge nor the corner were valid portal blocks.
					return false;
				}
			}
		}

		if ((Width[0] != Width[2]) || (Width[1] != Width[3]))
		{
			// Mismatched Portal Dimensions.
			return false;
		}
		if ((Width[0] < MIN_PORTAL_WIDTH) || (Width[1] < MIN_PORTAL_WIDTH))
		{
			// Portal too small.
			return false;
		}

		// LOG("NW corner (low corner) %d %d %d", Corner.x, Corner.y, Corner.z);
		// LOG("%d by %d", Width[0], Width[1]);
		for (int i = 0; i < Width[0]; i++)
		{
			for (int j = 0; j < Width[1]; j++)
			{
				a_ChunkInterface.SetBlock(Corner.x + i, Corner.y, Corner.z + j, E_BLOCK_END_PORTAL, 0);
				// TODO: Create block entity so portal doesn't become invisible on relog.
			}
		}
		return true;
	}





	/** Return true if this block is a portal frame, has an eye, and is facing the correct direction. */
	bool IsValidFrame(BLOCKTYPE a_BlockType, NIBBLETYPE a_BlockMeta, NIBBLETYPE a_ShouldFace)
	{
		return ((a_BlockType == E_BLOCK_END_PORTAL_FRAME)
			&& (a_BlockMeta == (a_ShouldFace | E_META_END_PORTAL_EYE))
		);
	}




	/** Return true if this block is a portal frame. */
	bool IsPortalFrame(BLOCKTYPE BlockType)
	{
		return (BlockType == E_BLOCK_END_PORTAL_FRAME);
	}





	virtual bool IsClickedThrough(void) override
	{
		// TODO: Colision
		return true;
	}





	virtual ColourID GetMapBaseColourID(NIBBLETYPE a_Meta) override
	{
		UNUSED(a_Meta);
		return 27;
	}
};




